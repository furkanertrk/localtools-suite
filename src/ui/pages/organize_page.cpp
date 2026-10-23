#include "organize_page.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QButtonGroup>
#include <QFrame>
#include <QScrollArea>
#include <QFileInfo>
#include <QLineEdit>
#include <fstream>
#include "../../core/backend/pdfium_backend.h"
#include "../../services/file_service.h"
#include "../../services/settings_service.h"
#include "../../services/logging_service.h"
#include "../common/icon_utils.h"
#include "../common/async_task.h"
#include "../common/output_file_dialog.h"
#include "../theme.h"

namespace LocalTools::UI {

namespace {
struct LoadedPdfPage {
    int index{0};
    int rotation{0};
    QImage thumbnail;
};

struct LoadedPdfDocument {
    QString path;
    std::vector<LoadedPdfPage> pages;
};

struct OrganizeLoadResult {
    std::vector<LoadedPdfDocument> documents;
    QStringList failedFiles;
    bool cancelled{false};
};
}

OrganizePage::OrganizePage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

OrganizePage::~OrganizePage() {
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void OrganizePage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Action Toolbar at the very top (wrapped in scroll area for full responsiveness)
    auto *toolbarScrollArea = new QScrollArea(this);
    toolbarScrollArea->setObjectName("ToolbarScrollArea");
    toolbarScrollArea->setWidgetResizable(true);
    toolbarScrollArea->setFrameShape(QFrame::NoFrame);
    toolbarScrollArea->setFixedHeight(46);
    toolbarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarScrollArea->setStyleSheet("QScrollArea#ToolbarScrollArea, QScrollArea#ToolbarScrollArea > QWidget > QWidget { background: transparent; border: none; }");

    auto *toolbarContainer = new QWidget(toolbarScrollArea);
    toolbarContainer->setObjectName("ToolbarContainer");
    auto *toolbarLayout = new QHBoxLayout(toolbarContainer);
    toolbarLayout->setSpacing(8);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    auto createTabBtn = [this](const QString& text, IconType iconType, int id) -> QPushButton* {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::TextSecondary), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_tabGroup->addButton(btn, id);
        return btn;
    };

    m_mergeBtn = createTabBtn(tr("Merge"), IconType::Merge, 1);
    m_splitBtn = createTabBtn(tr("Split"), IconType::Split, 2);
    m_extractBtn = createTabBtn(tr("Extract Pages"), IconType::Extract, 3);
    m_deleteBtn = createTabBtn(tr("Delete Pages"), IconType::Delete, 4);
    m_reorderBtn = createTabBtn(tr("Reorder"), IconType::Reorder, 5);
    m_rotateBtn = createTabBtn(tr("Rotate"), IconType::Rotate, 6);

    m_mergeBtn->setChecked(true); // Default active tab
    m_selectAllBtn = new QPushButton(this);
    m_clearBtn = new QPushButton(this);
    m_selectAllBtn->setObjectName("ToolTabButton");
    m_clearBtn->setObjectName("ToolTabButton");
    m_selectAllBtn->setIcon(IconUtils::getIcon(IconType::Success, Theme::color(ColorToken::TextSecondary), 14));
    m_selectAllBtn->setIconSize(QSize(14, 14));
    m_clearBtn->setIcon(IconUtils::getIcon(IconType::Delete, Theme::color(ColorToken::TextSecondary), 14));
    m_clearBtn->setIconSize(QSize(14, 14));
    m_selectAllBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setCursor(Qt::PointingHandCursor);

    toolbarLayout->addWidget(m_mergeBtn);
    toolbarLayout->addWidget(m_splitBtn);
    toolbarLayout->addWidget(m_extractBtn);
    toolbarLayout->addWidget(m_deleteBtn);
    toolbarLayout->addWidget(m_reorderBtn);
    toolbarLayout->addWidget(m_rotateBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_selectAllBtn);
    toolbarLayout->addWidget(m_clearBtn);

    toolbarScrollArea->setWidget(toolbarContainer);
    mainLayout->addWidget(toolbarScrollArea);

    // 2. Tab Description Subtitle
    m_descLabel = new QLabel(tr("Merge your uploaded PDF documents into a single file in any order."), this);
    m_descLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: %1; margin-bottom: 2px;")
                                   .arg(Theme::hex(ColorToken::TextSecondary)));
    m_descLabel->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_descLabel);

    // Connect Tab Switch to Header Update
    connect(m_tabGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setActiveTab(id);
    });

    // Status Banner — after toolbar, hidden by default (takes zero space when hidden)
    m_statusBanner = new StatusBannerWidget(this);
    m_statusBanner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusBanner);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // DropZone
    m_dropZone = new DropZoneWidget(this);
    m_dropZone->setInstruction(tr("Drag PDF Files Here"), tr("Merge, split, or organize PDF pages"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &OrganizePage::loadPdfFiles);
    mainLayout->addWidget(m_dropZone);

    // Stacked Widget for Workspace vs Result Screen
    m_stackedWidget = new QStackedWidget(this);

    // Workspace Container
    auto *workspaceContainer = new QWidget(this);
    auto *workspaceLayout = new QVBoxLayout(workspaceContainer);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);

    m_pageWorkspace = new PageWorkspace(workspaceContainer);
    m_thumbnailGrid = m_pageWorkspace->grid();
    workspaceLayout->addWidget(m_pageWorkspace, 1);

    // Bottom Bar with Execute Button & Capability-Driven Toolbars
    auto *bottomLayout = new QHBoxLayout();
    m_selectionInfoLabel = new QLabel(tr("No pages selected yet"), workspaceContainer);
    m_selectionInfoLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;")
                                             .arg(Theme::hex(ColorToken::TextSecondary)));
    bottomLayout->addWidget(m_selectionInfoLabel);

    // Reorder Action Buttons
    m_moveLeftBtn = new QPushButton(workspaceContainer);
    m_moveRightBtn = new QPushButton(workspaceContainer);
    m_moveLeftBtn->setObjectName("ToolTabButton");
    m_moveRightBtn->setObjectName("ToolTabButton");
    m_moveLeftBtn->setCursor(Qt::PointingHandCursor);
    m_moveRightBtn->setCursor(Qt::PointingHandCursor);
    m_moveLeftBtn->hide();
    m_moveRightBtn->hide();
    bottomLayout->addWidget(m_moveLeftBtn);
    bottomLayout->addWidget(m_moveRightBtn);

    // Rotate Action Buttons
    m_rotateLeftBtn = new QPushButton(workspaceContainer);
    m_rotateRightBtn = new QPushButton(workspaceContainer);
    m_rotate180Btn = new QPushButton(workspaceContainer);
    m_rotateLeftBtn->setObjectName("ToolTabButton");
    m_rotateRightBtn->setObjectName("ToolTabButton");
    m_rotate180Btn->setObjectName("ToolTabButton");
    m_rotateLeftBtn->setCursor(Qt::PointingHandCursor);
    m_rotateRightBtn->setCursor(Qt::PointingHandCursor);
    m_rotate180Btn->setCursor(Qt::PointingHandCursor);
    m_rotateLeftBtn->hide();
    m_rotateRightBtn->hide();
    m_rotate180Btn->hide();
    bottomLayout->addWidget(m_rotateLeftBtn);
    bottomLayout->addWidget(m_rotateRightBtn);
    bottomLayout->addWidget(m_rotate180Btn);

    // Split options live in a bounded card below the page preview. Keeping the
    // radio buttons inside compact rows prevents their keyboard-focus border
    // from stretching across the entire workspace.
    m_splitOptionsWidget = new QFrame(workspaceContainer);
    m_splitOptionsWidget->setObjectName("SplitOptionsCard");
    m_splitOptionsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_splitOptionsWidget->setMaximumWidth(920);
    auto *splitVLayout = new QVBoxLayout(m_splitOptionsWidget);
    splitVLayout->setContentsMargins(12, 10, 12, 10);
    splitVLayout->setSpacing(Theme::space(Space::XXS));

    auto createSplitRow = [this]() {
        auto *row = new QWidget(m_splitOptionsWidget);
        row->setObjectName("SplitOptionRow");
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->setMinimumHeight(38);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->setSpacing(Theme::space(Space::S));
        return qMakePair(row, layout);
    };

    auto *splitModeGroup = new QButtonGroup(m_splitOptionsWidget);
    splitModeGroup->setExclusive(true);

    const auto splitAllRow = createSplitRow();
    m_splitAllRadio = new QRadioButton(tr("Split all pages into single-page PDFs (1 page / file)"), splitAllRow.first);
    m_splitAllRadio->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    splitAllRow.second->addWidget(m_splitAllRadio, 0, Qt::AlignVCenter);
    splitAllRow.second->addStretch();

    const auto splitNRow = createSplitRow();
    m_splitNRadio = new QRadioButton(tr("Create a new PDF every N pages:"), splitNRow.first);
    m_splitNRadio->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_splitNSpin = new QSpinBox(splitNRow.first);
    m_splitNSpin->setRange(2, 100);
    m_splitNSpin->setValue(2);
    m_splitNSpin->setMinimumWidth(90);
    m_splitNSpin->setMaximumWidth(120);
    m_splitNSpin->setEnabled(false);
    splitNRow.second->addWidget(m_splitNRadio, 0, Qt::AlignVCenter);
    splitNRow.second->addWidget(m_splitNSpin, 0, Qt::AlignVCenter);
    splitNRow.second->addStretch();

    const auto splitRangeRow = createSplitRow();
    m_splitRangeRadio = new QRadioButton(tr("Split by custom page ranges:"), splitRangeRow.first);
    m_splitRangeRadio->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_splitRangeEdit = new QLineEdit(splitRangeRow.first);
    m_splitRangeEdit->setPlaceholderText(tr("e.g. 1-3, 5, 8-10"));
    m_splitRangeEdit->setMinimumWidth(240);
    m_splitRangeEdit->setMaximumWidth(420);
    m_splitRangeEdit->setEnabled(false);
    splitRangeRow.second->addWidget(m_splitRangeRadio, 0, Qt::AlignVCenter);
    splitRangeRow.second->addWidget(m_splitRangeEdit, 1, Qt::AlignVCenter);
    splitRangeRow.second->addStretch();

    splitModeGroup->addButton(m_splitAllRadio);
    splitModeGroup->addButton(m_splitNRadio);
    splitModeGroup->addButton(m_splitRangeRadio);

    m_splitAllRadio->setChecked(true);

    splitVLayout->addWidget(splitAllRow.first);
    splitVLayout->addWidget(splitNRow.first);
    splitVLayout->addWidget(splitRangeRow.first);

    connect(m_splitAllRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_splitNSpin->setEnabled(false);
            m_splitRangeEdit->setEnabled(false);
        }
    });
    connect(m_splitNRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_splitNSpin->setEnabled(checked);
        if (checked) m_splitRangeEdit->setEnabled(false);
    });
    connect(m_splitRangeRadio, &QRadioButton::toggled, this, [this](bool checked) {
        m_splitRangeEdit->setEnabled(checked);
        if (checked) m_splitNSpin->setEnabled(false);
    });

    m_splitOptionsWidget->hide();
    workspaceLayout->addWidget(m_splitOptionsWidget);

    bottomLayout->addStretch();

    m_executeBtn = new QPushButton(tr("Start Operation"), workspaceContainer);
    m_executeBtn->setObjectName("PrimaryButton");
    // Minimum, not fixed: Turkish labels run up to ~1.6x the English width.
    m_executeBtn->setMinimumSize(190, 40);
    m_executeBtn->setCursor(Qt::PointingHandCursor);
    bottomLayout->addWidget(m_executeBtn);
    
    workspaceLayout->addLayout(bottomLayout);
    m_stackedWidget->addWidget(workspaceContainer);

    // Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    m_stackedWidget->addWidget(m_resultScreen);

    m_stackedWidget->hide();
    mainLayout->addWidget(m_stackedWidget, 1);

    // Signal Connections
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &OrganizePage::clearWorkspace);

    connect(m_thumbnailGrid, &ThumbnailGridWidget::selectionCountChanged, this, [this](int selected, int total) {
        updateControlsState(selected, total);
    });

    connect(m_moveLeftBtn, &QPushButton::clicked, this, [this]() {
        if (!m_thumbnailGrid->moveSelectedPage(-1)) {
            m_statusBanner->showBanner(BannerType::Warning, tr("Selected page cannot be moved further left."));
        }
    });
    connect(m_moveRightBtn, &QPushButton::clicked, this, [this]() {
        if (!m_thumbnailGrid->moveSelectedPage(1)) {
            m_statusBanner->showBanner(BannerType::Warning, tr("Selected page cannot be moved further right."));
        }
    });

    connect(m_rotateLeftBtn, &QPushButton::clicked, this, [this]() {
        m_thumbnailGrid->rotateSelectedPages(-90);
    });
    connect(m_rotateRightBtn, &QPushButton::clicked, this, [this]() {
        m_thumbnailGrid->rotateSelectedPages(90);
    });
    connect(m_rotate180Btn, &QPushButton::clicked, this, [this]() {
        m_thumbnailGrid->rotateSelectedPages(180);
    });

    connect(m_selectAllBtn, &QPushButton::clicked, this, [this]() {
        auto selectedCount = Core::State::WorkspaceState::instance().getSelectedPages().size();
        auto totalCount = Core::State::WorkspaceState::instance().getAllPages().size();
        if (selectedCount == totalCount && totalCount > 0) {
            m_thumbnailGrid->deselectAll();
        } else {
            m_thumbnailGrid->selectAll();
        }
    });

    connect(m_clearBtn, &QPushButton::clicked, this, &OrganizePage::clearWorkspace);

    // Execute Logic
    connect(m_executeBtn, &QPushButton::clicked, this, [this]() {
        int activeId = m_tabGroup->checkedId();
        if (activeId == -1) return;

        if (m_loadedFiles.empty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("Please upload a PDF file first to perform operation."));
            return;
        }

        QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
        auto *worker = new Workers::PdfWorker();

        if (activeId == 1) { // Merge
            if (m_loadedFiles.size() < 2) {
                m_statusBanner->showBanner(BannerType::Warning, tr("Upload at least 2 PDF files for merge operation."));
                worker->deleteLater();
                return;
            }
            QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Merged PDF"), defaultDir + "/merged.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
            if (targetFile.isEmpty()) { worker->deleteLater(); return; }
            worker->prepareMerge(m_loadedFiles, targetFile);
            runWorkerOperation(worker, targetFile);

        } else if (activeId == 2) { // Split
            if (m_splitRangeRadio && m_splitRangeRadio->isChecked()) {
                QString rangeStr = m_splitRangeEdit ? m_splitRangeEdit->text().trimmed() : QString();
                if (rangeStr.isEmpty()) {
                    m_statusBanner->showBanner(BannerType::Warning, tr("Please enter custom page range (e.g. 1-3, 5, 8-10)."));
                    worker->deleteLater();
                    return;
                }
                QList<int> pagesToExtract;
                QStringList parts = rangeStr.split(',', Qt::SkipEmptyParts);
                int totalPages = m_thumbnailGrid->getPageCount();
                for (QString p : parts) {
                    p = p.trimmed();
                    if (p.contains('-')) {
                        QStringList sub = p.split('-');
                        if (sub.size() == 2) {
                            int start = sub[0].toInt();
                            int end = sub[1].toInt();
                            for (int i = start; i <= end; ++i) {
                                int idx = i - 1;
                                if (idx >= 0 && idx < totalPages && !pagesToExtract.contains(idx)) {
                                    pagesToExtract.append(idx);
                                }
                            }
                        }
                    } else {
                        int val = p.toInt();
                        int idx = val - 1;
                        if (idx >= 0 && idx < totalPages && !pagesToExtract.contains(idx)) {
                            pagesToExtract.append(idx);
                        }
                    }
                }
                if (pagesToExtract.isEmpty()) {
                    m_statusBanner->showBanner(BannerType::Warning, tr("Entered page range does not match valid page numbers in document."));
                    worker->deleteLater();
                    return;
                }
                QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Selected Page Range"), defaultDir + "/split_range.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
                if (targetFile.isEmpty()) { worker->deleteLater(); return; }
                worker->prepareExtract(m_loadedFiles.first(), pagesToExtract, targetFile);
                runWorkerOperation(worker, targetFile);
            } else {
                QString targetDir = QFileDialog::getExistingDirectory(this, tr("Select Folder for Split PDFs"), defaultDir);
                if (targetDir.isEmpty()) { worker->deleteLater(); return; }
                int pagesPerFile = (m_splitNRadio && m_splitNRadio->isChecked()) ? m_splitNSpin->value() : 1;
                worker->prepareSplit(m_loadedFiles.first(), targetDir, pagesPerFile);
                runWorkerOperation(worker, targetDir);
            }

        } else if (activeId == 3) { // Extract
            auto selected = m_thumbnailGrid->getSelectedPageIndices();
            if (selected.empty()) {
                m_statusBanner->showBanner(BannerType::Warning, tr("Please select at least one page to extract."));
                worker->deleteLater();
                return;
            }
            QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Selected Pages"), defaultDir + "/extracted_pages.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
            if (targetFile.isEmpty()) { worker->deleteLater(); return; }
            QList<int> qSelected(selected.begin(), selected.end());
            worker->prepareExtract(m_loadedFiles.first(), qSelected, targetFile);
            runWorkerOperation(worker, targetFile);

        } else if (activeId == 4) { // Delete
            auto selected = m_thumbnailGrid->getSelectedPageIndices();
            if (selected.empty()) {
                m_statusBanner->showBanner(BannerType::Warning, tr("Please select at least one page to delete."));
                worker->deleteLater();
                return;
            }
            if (static_cast<int>(selected.size()) >= m_thumbnailGrid->getPageCount()) {
                m_statusBanner->showBanner(BannerType::Warning, tr("You cannot delete all pages."));
                worker->deleteLater();
                return;
            }
            QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save PDF After Deletion"), defaultDir + "/document_edited.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
            if (targetFile.isEmpty()) { worker->deleteLater(); return; }
            QList<int> qSelected(selected.begin(), selected.end());
            worker->prepareDelete(m_loadedFiles.first(), qSelected, targetFile);
            runWorkerOperation(worker, targetFile);

        } else if (activeId == 5) { // Reorder
            if (m_loadedFiles.size() != 1) {
                m_statusBanner->showBanner(BannerType::Warning, tr("Upload a single PDF file for page reordering."));
                worker->deleteLater();
                return;
            }
            const auto pageOrder = m_thumbnailGrid->getPageOrder();
            if (pageOrder.empty()) {
                m_statusBanner->showBanner(BannerType::Warning, tr("No pages found to reorder."));
                worker->deleteLater();
                return;
            }
            QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Reordered PDF"), defaultDir + "/document_reordered.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
            if (targetFile.isEmpty()) { worker->deleteLater(); return; }
            QList<int> orderedPages(pageOrder.begin(), pageOrder.end());
            worker->prepareReorder(m_loadedFiles.first(), orderedPages, targetFile);
            runWorkerOperation(worker, targetFile);

        } else if (activeId == 6) { // Rotate
            auto rotations = m_thumbnailGrid->getPageRotations();
            if (rotations.empty()) {
                auto selected = m_thumbnailGrid->getSelectedPageIndices();
                if (selected.empty()) {
                    m_statusBanner->showBanner(BannerType::Warning, tr("Please select pages to rotate."));
                    worker->deleteLater();
                    return;
                }
                Core::State::WorkspaceState::instance().rotateSelectedPages(90);
                rotations = m_thumbnailGrid->getPageRotations();
            }
            
            QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Rotated PDF"), defaultDir + "/document_rotated.pdf", tr("PDF Files (*.pdf)"), m_loadedFiles);
            if (targetFile.isEmpty()) { worker->deleteLater(); return; }
            
            QList<QPair<int, int>> qRotations;
            for (const auto& r : rotations) qRotations.append({r.first, r.second});
            
            worker->prepareRotate(m_loadedFiles.first(), qRotations, targetFile);
            runWorkerOperation(worker, targetFile);

        }
    });
    retranslateUi();
    refreshThemeIcons();
}

void OrganizePage::clearWorkspace() {
    ++m_loadGeneration;
    m_loadedFiles.clear();
    Core::State::WorkspaceState::instance().clear();

    if (m_stackedWidget) {
        m_stackedWidget->hide();
    }
    if (m_dropZone) {
        m_dropZone->show();
    }
    if (m_statusBanner) {
        m_statusBanner->hideBanner();
    }
    if (m_progressBar) {
        m_progressBar->hide();
    }
    if (m_selectAllBtn) {
        const int activeTab = m_tabGroup ? m_tabGroup->checkedId() : 1;
        m_selectAllBtn->setVisible(activeTab == 3 || activeTab == 4 || activeTab == 5 || activeTab == 6);
    }
    updateControlsState(0, 0);
}

void OrganizePage::refreshThemeIcons() {
    const QColor secondaryText = Theme::color(ColorToken::TextSecondary);

    if (m_mergeBtn) m_mergeBtn->setIcon(IconUtils::getIcon(IconType::Merge, secondaryText, 16));
    if (m_splitBtn) m_splitBtn->setIcon(IconUtils::getIcon(IconType::Split, secondaryText, 16));
    if (m_extractBtn) m_extractBtn->setIcon(IconUtils::getIcon(IconType::Extract, secondaryText, 16));
    if (m_deleteBtn) m_deleteBtn->setIcon(IconUtils::getIcon(IconType::Delete, secondaryText, 16));
    if (m_reorderBtn) m_reorderBtn->setIcon(IconUtils::getIcon(IconType::Reorder, secondaryText, 16));
    if (m_rotateBtn) m_rotateBtn->setIcon(IconUtils::getIcon(IconType::Rotate, secondaryText, 16));
    if (m_selectAllBtn) m_selectAllBtn->setIcon(IconUtils::getIcon(IconType::Success, secondaryText, 14));
    if (m_clearBtn) m_clearBtn->setIcon(IconUtils::getIcon(IconType::Delete, secondaryText, 14));

    if (m_descLabel) {
        m_descLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: %1; margin-bottom: 2px;")
                                       .arg(Theme::hex(ColorToken::TextSecondary)));
    }
    if (m_selectionInfoLabel) {
        m_selectionInfoLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;")
                                                 .arg(Theme::hex(ColorToken::TextSecondary)));
    }
}

void OrganizePage::updateControlsState(int selected, int total) {
    if (!m_tabGroup || !m_selectionInfoLabel || !m_executeBtn) return;

    const int activeTab = m_tabGroup->checkedId();
    switch (activeTab) {
        case 1: // Merge
            m_selectionInfoLabel->setText(tr("%1 PDF document(s) uploaded for merging.").arg(m_loadedFiles.size()));
            m_executeBtn->setText(tr("Merge PDFs"));
            break;
        case 2: // Split
            m_selectionInfoLabel->setText(tr("Total of %1 page(s) will be split.").arg(total));
            m_executeBtn->setText(tr("Split PDF"));
            break;
        case 3: // Extract
            m_selectionInfoLabel->setText(selected > 0 ? tr("%1 page(s) selected").arg(selected) : tr("Please select pages to extract"));
            m_executeBtn->setText(tr("Extract Selected Pages"));
            break;
        case 4: // Delete
            m_selectionInfoLabel->setText(selected > 0 ? tr("%1 page(s) selected for deletion").arg(selected) : tr("Please select pages to delete"));
            m_executeBtn->setText(tr("Delete Selected Pages"));
            break;
        case 5: // Reorder
            m_selectionInfoLabel->setText(selected == 1 ? tr("1 page selected — Move with arrow keys or buttons") : tr("Select 1 page to reorder"));
            m_executeBtn->setText(tr("Save New Order"));
            break;
        case 6: // Rotate
            m_selectionInfoLabel->setText(selected > 0 ? tr("%1 page(s) selected for rotation").arg(selected) : tr("Select pages you want to rotate"));
            m_executeBtn->setText(tr("Save Rotated PDF"));
            break;
        default:
            m_selectionInfoLabel->setText(tr("No pages selected yet"));
            m_executeBtn->setText(tr("Start Operation"));
            break;
    }

    if (m_selectAllBtn) {
        m_selectAllBtn->setText(selected == total && total > 0 ? tr("Clear Selection") : tr("Select All"));
    }

    const bool canMove = activeTab == 5 && selected == 1;
    if (m_moveLeftBtn) m_moveLeftBtn->setEnabled(canMove);
    if (m_moveRightBtn) m_moveRightBtn->setEnabled(canMove);

    const bool canRotate = activeTab == 6 && selected > 0;
    if (m_rotateLeftBtn) m_rotateLeftBtn->setEnabled(canRotate);
    if (m_rotateRightBtn) m_rotateRightBtn->setEnabled(canRotate);
    if (m_rotate180Btn) m_rotate180Btn->setEnabled(canRotate);
}

void OrganizePage::updateTabHeader(int tabId) {
    if (!m_descLabel) return;
    switch (tabId) {
        case 1:
            m_descLabel->setText(tr("Merge your uploaded PDF documents into a single file in any order."));
            break;
        case 2:
            m_descLabel->setText(tr("Split your PDF document into separate parts based on specific page ranges."));
            break;
        case 3:
            m_descLabel->setText(tr("Create a new PDF by extracting only selected pages from your document."));
            break;
        case 4:
            m_descLabel->setText(tr("Completely remove unwanted pages from your document."));
            break;
        case 5:
            m_descLabel->setText(tr("Move selected pages left or right within a single PDF; then save as a new file."));
            break;
        case 6:
            m_descLabel->setText(tr("Rotate selected pages clockwise or counter-clockwise."));
            break;
        default:
            break;
    }
}

void OrganizePage::setActiveTab(int toolId) {
    const int targetId = toolId >= 1 && toolId <= 6 ? toolId : 1;

    if (QAbstractButton* btn = m_tabGroup->button(targetId)) {
        btn->setChecked(true);
    }
    updateTabHeader(targetId);

    const bool isReorder = (targetId == 5);
    const bool isRotate = (targetId == 6);
    const bool isSplit = (targetId == 2);

    if (m_thumbnailGrid) {
        static constexpr ThumbnailActionMode actionModes[] = {
            ThumbnailActionMode::Merge,
            ThumbnailActionMode::Merge,
            ThumbnailActionMode::Split,
            ThumbnailActionMode::Extract,
            ThumbnailActionMode::Delete,
            ThumbnailActionMode::Reorder,
            ThumbnailActionMode::Rotate
        };
        m_thumbnailGrid->setActionMode(actionModes[targetId]);
    }

    m_moveLeftBtn->setVisible(isReorder);
    m_moveRightBtn->setVisible(isReorder);

    m_rotateLeftBtn->setVisible(isRotate);
    m_rotateRightBtn->setVisible(isRotate);
    m_rotate180Btn->setVisible(isRotate);

    m_splitOptionsWidget->setVisible(isSplit);

    m_selectAllBtn->setVisible(targetId == 3 || targetId == 4 || targetId == 5 || targetId == 6);

    if (m_thumbnailGrid) {
        const auto selected = m_thumbnailGrid->getSelectedPageIndices().size();
        updateControlsState(static_cast<int>(selected), m_thumbnailGrid->getPageCount());
    }

    if (!m_loadedFiles.empty()) {
        m_thumbnailGrid->show();
    }
}

void OrganizePage::loadPdfFiles(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    // Purge any existing state first
    m_loadedFiles.clear();
    Core::State::WorkspaceState::instance().clear();

    const quint64 generation = ++m_loadGeneration;
    m_loadedFiles = filePaths;
    m_dropZone->hide();
    
    if (m_stackedWidget && m_stackedWidget->count() > 0) {
        m_stackedWidget->setCurrentIndex(0);
        m_stackedWidget->show();
    }

    m_thumbnailGrid->show();
    const int activeTab = m_tabGroup ? m_tabGroup->checkedId() : 1;
    m_selectAllBtn->setVisible(activeTab == 3 || activeTab == 4 || activeTab == 5 || activeTab == 6);

    if (m_statusBanner) m_statusBanner->hideBanner();

    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_executeBtn->setEnabled(false);

    AsyncTask::run<OrganizeLoadResult>(
        this,
        [filePaths](AsyncTask::CancelCheck cancelled) {
            OrganizeLoadResult result;
            for (const QString& path : filePaths) {
                if (cancelled()) {
                    result.cancelled = true;
                    break;
                }
                Core::PdfiumBackend backend;
                if (!backend.openDocument(path.toStdString())) {
                    result.failedFiles.append(path);
                    continue;
                }
                LoadedPdfDocument document;
                document.path = path;
                const int pageCount = backend.getPageCount();
                document.pages.reserve(static_cast<size_t>(pageCount));
                for (int i = 0; i < pageCount; ++i) {
                    if (cancelled()) {
                        result.cancelled = true;
                        break;
                    }
                    document.pages.push_back({i, backend.getPageRotation(i),
                                               backend.renderPage(i, 200, 300)});
                }
                backend.closeDocument();
                if (result.cancelled) break;
                result.documents.push_back(std::move(document));
            }
            return result;
        },
        [this, generation](OrganizeLoadResult result) {
            if (generation != m_loadGeneration) return;

            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            auto& state = Core::State::WorkspaceState::instance();
            state.clear();
            m_loadedFiles.clear();

            for (const auto& loadedDocument : result.documents) {
                Core::State::DocumentState document;
                document.id = loadedDocument.path;
                document.filePath = loadedDocument.path;
                document.totalPages = static_cast<int>(loadedDocument.pages.size());
                document.pages.reserve(loadedDocument.pages.size());
                for (const auto& loadedPage : loadedDocument.pages) {
                    Core::State::PageState page;
                    page.id = loadedDocument.path + ":" + QString::number(loadedPage.index);
                    page.filePath = loadedDocument.path;
                    page.pageIndex = loadedPage.index;
                    page.rotation = loadedPage.rotation;
                    page.thumbnail = QPixmap::fromImage(loadedPage.thumbnail);
                    document.pages.push_back(std::move(page));
                }
                state.addDocument(document);
                m_loadedFiles.append(loadedDocument.path);
            }

            if (!m_loadedFiles.isEmpty()) {
                m_thumbnailGrid->loadFromState();
                updateControlsState(0, m_thumbnailGrid->getPageCount());
                Services::LoggingService::instance().info(
                    "OrganizePage", "Loaded PDF files into workspace.");
            } else if (!result.cancelled) {
                m_stackedWidget->hide();
                m_dropZone->show();
            }

            if (!result.failedFiles.isEmpty()) {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    tr("%1 PDF file(s) could not be opened. They may be corrupted or password protected.")
                        .arg(result.failedFiles.size()));
            } else if (!result.cancelled) {
                m_statusBanner->hideBanner();
            }
            m_executeBtn->setEnabled(!m_loadedFiles.isEmpty());
        });
}

void OrganizePage::runWorkerOperation(Workers::PdfWorker* worker, const QString& expectedOutputFile) {
    if (m_workerThread && m_workerThread->isRunning()) return;

    m_workerThread = new QThread(this);
    worker->moveToThread(m_workerThread);

    m_progressBar->setValue(0);
    m_progressBar->show();

    // Disable all buttons in toolbar to prevent interaction during processing
    const auto buttons = m_tabGroup->buttons();
    for (auto* btn : buttons) btn->setEnabled(false);
    m_selectAllBtn->setEnabled(false);
    m_clearBtn->setEnabled(false);

    connect(m_workerThread, &QThread::started, worker, &Workers::PdfWorker::process);

    connect(worker, &Workers::PdfWorker::progressChanged, this, [this](int value) {
        m_progressBar->setValue(value);
    });

    connect(worker, &Workers::PdfWorker::statusMessage, this, [this](const QString& msg) {
        m_statusBanner->showBanner(BannerType::Info, msg);
    });

    connect(worker, &Workers::PdfWorker::finished, this, [this, worker, expectedOutputFile](const Core::Result<void>& res) {
        m_progressBar->hide();
        m_workerThread->quit();
        m_workerThread->wait();
        worker->deleteLater();

        // Re-enable toolbar buttons
        const auto buttons = m_tabGroup->buttons();
        for (auto* btn : buttons) btn->setEnabled(true);
        m_selectAllBtn->setEnabled(true);
        m_clearBtn->setEnabled(true);

        if (res.hasValue()) {
            m_statusBanner->hideBanner();
            m_resultScreen->showResult(expectedOutputFile, tr("Operation Completed Successfully!"));
            m_stackedWidget->setCurrentWidget(m_resultScreen);
        } else {
            m_statusBanner->showBanner(BannerType::Error, tr("Operation failed: %1").arg(QString::fromStdString(res.error().message)));
        }
    });

    m_workerThread->start();
}

void OrganizePage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void OrganizePage::retranslateUi() {
    if (m_mergeBtn) m_mergeBtn->setText(tr("Merge"));
    if (m_splitBtn) m_splitBtn->setText(tr("Split"));
    if (m_extractBtn) m_extractBtn->setText(tr("Extract Pages"));
    if (m_deleteBtn) m_deleteBtn->setText(tr("Delete Pages"));
    if (m_reorderBtn) m_reorderBtn->setText(tr("Reorder"));
    if (m_rotateBtn) m_rotateBtn->setText(tr("Rotate"));
    if (m_clearBtn) m_clearBtn->setText(tr("Clear"));
    if (m_moveLeftBtn) m_moveLeftBtn->setText(tr("← Move Left"));
    if (m_moveRightBtn) m_moveRightBtn->setText(tr("Move Right →"));
    if (m_rotateLeftBtn) m_rotateLeftBtn->setText(tr("Rotate 90° Left"));
    if (m_rotateRightBtn) m_rotateRightBtn->setText(tr("Rotate 90° Right"));
    if (m_rotate180Btn) m_rotate180Btn->setText(tr("Rotate 180°"));

    if (m_splitAllRadio) m_splitAllRadio->setText(tr("Split all pages into single-page PDFs (1 page / file)"));
    if (m_splitNRadio) m_splitNRadio->setText(tr("Create a new PDF every N pages:"));
    if (m_splitRangeRadio) m_splitRangeRadio->setText(tr("Split by custom page ranges:"));
    if (m_splitRangeEdit) m_splitRangeEdit->setPlaceholderText(tr("e.g. 1-3, 5, 8-10"));

    if (m_dropZone) {
        m_dropZone->setInstruction(tr("Drag PDF Files Here"),
                                   tr("Merge, split, or organize PDF pages"));
    }

    if (m_tabGroup && m_tabGroup->checkedButton()) {
        updateTabHeader(m_tabGroup->checkedId());
    }

    if (m_thumbnailGrid) {
        const auto selected = m_thumbnailGrid->getSelectedPageIndices().size();
        updateControlsState(static_cast<int>(selected), m_thumbnailGrid->getPageCount());
    }
}

} // namespace LocalTools::UI
