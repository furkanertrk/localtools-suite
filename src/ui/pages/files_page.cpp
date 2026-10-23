#include "files_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

#include "../../services/settings_service.h"
#include "../../services/logging_service.h"
#include "../common/async_task.h"

namespace LocalTools::UI {

FilesPage::FilesPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void FilesPage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_pageStack = new QStackedWidget(this);

    auto *mainContainer = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Top Sub-navigation Tabs
    mainLayout->addWidget(createTabHeader());

    // 2. Tab Description Subtitle
    m_tabDescriptionLabel = new QLabel(tr("Calculate and verify MD5, SHA-1, SHA-256, and SHA-512 checksum values of any file."), this);
    m_tabDescriptionLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    mainLayout->addWidget(m_tabDescriptionLabel);

    // 3. Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    mainLayout->addWidget(m_statusBanner);

    // 3. Panels Stack
    m_panelStack = new QStackedWidget(this);
    m_hashPanel = createHashPanel();
    m_panelStack->addWidget(m_hashPanel);
    mainLayout->addWidget(m_panelStack);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    mainLayout->addStretch();
    m_pageStack->addWidget(mainContainer);

    // 4. Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &FilesPage::resetWorkspace);
    m_pageStack->addWidget(m_resultScreen);

    rootLayout->addWidget(m_pageStack);
    retranslateUi();
}

QWidget* FilesPage::createTabHeader() {
    auto *headerWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(headerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_tabButtonGroup = new QButtonGroup(this);
    m_tabButtonGroup->setExclusive(true);

    auto createTabBtn = [this, layout](const QString& text, IconType iconType, int id) {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setIcon(IconUtils::getIcon(iconType, QColor("#64748b"), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_tabButtonGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    m_hashTabBtn = createTabBtn(tr("Calculate Hash"), IconType::Success, 0);
    m_hashTabBtn->setChecked(true);
    layout->addStretch();

    connect(m_tabButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_panelStack->setCurrentIndex(id);
    });

    return headerWidget;
}

QWidget* FilesPage::createHashPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    m_hashDropZone = new DropZoneWidget(card);
    m_hashDropZone->setInstruction(tr("Drop Any File to Calculate Hash"),
                                   tr("or select a file from your computer"));
    m_hashDropZone->setAcceptedFileExtensions({}, tr("All Files"), QString());
    connect(m_hashDropZone, &DropZoneWidget::filesDropped, this, &FilesPage::handleHashFileDropped);
    layout->addWidget(m_hashDropZone);

    m_hashResultContainer = new QWidget(card);
    auto *resLayout = new QVBoxLayout(m_hashResultContainer);
    resLayout->setContentsMargins(0, 0, 0, 0);
    resLayout->setSpacing(8);

    m_hashFileNameLabel = new QLabel(m_hashResultContainer);
    m_hashFileNameLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    resLayout->addWidget(m_hashFileNameLabel);

    auto addHashRow = [this, resLayout](const QString& label, QLineEdit*& edit) {
        auto *row = new QHBoxLayout();
        auto *lbl = new QLabel(label, this);
        lbl->setFixedWidth(70);
        lbl->setStyleSheet("font-weight: 600; font-size: 12px;");
        row->addWidget(lbl);

        edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setStyleSheet("font-family: monospace; font-size: 12px;");
        row->addWidget(edit, 1);

        auto *copyBtn = new QPushButton(tr("Copy"), this);
        copyBtn->setObjectName("ToolTabButton");
        copyBtn->setCursor(Qt::PointingHandCursor);
        connect(copyBtn, &QPushButton::clicked, this, [edit]() {
            QGuiApplication::clipboard()->setText(edit->text());
        });
        row->addWidget(copyBtn);
        resLayout->addLayout(row);
    };

    addHashRow("MD5:", m_md5Edit);
    addHashRow("SHA-1:", m_sha1Edit);
    addHashRow("SHA-256:", m_sha256Edit);
    addHashRow("SHA-512:", m_sha512Edit);

    resLayout->addSpacing(10);
    auto *verifyRow = new QHBoxLayout();
    m_verifyHashInput = new QLineEdit(this);
    m_verifyHashInput->setPlaceholderText(tr("Paste the expected Hash value here to verify..."));
    connect(m_verifyHashInput, &QLineEdit::textChanged, this, &FilesPage::checkHashMatch);
    verifyRow->addWidget(m_verifyHashInput, 1);

    m_verifyResultLabel = new QLabel(this);
    m_verifyResultLabel->setStyleSheet("font-weight: bold; padding-left: 10px;");
    verifyRow->addWidget(m_verifyResultLabel);
    resLayout->addLayout(verifyRow);

    m_hashResultContainer->hide();
    layout->addWidget(m_hashResultContainer);
    return card;
}

void FilesPage::setMode(FilesTabMode mode) {
    int id = static_cast<int>(mode);
    if (auto *btn = m_tabButtonGroup->button(id)) {
        btn->setChecked(true);
        m_panelStack->setCurrentIndex(id);
    }
}

void FilesPage::handleHashFileDropped(const QStringList& files) {
    if (files.isEmpty()) return;
    m_hashedFilePath = files.first();
    m_hashDropZone->hide();
    
    QFileInfo fi(m_hashedFilePath);
    m_hashFileNameLabel->setText(tr("Selected File: %1").arg(fi.fileName()));
    
    m_progressBar->setRange(0, 0);
    m_progressBar->show();

    // Run async
    m_md5Edit->setText(tr("Calculating..."));
    m_sha1Edit->setText(tr("Calculating..."));
    m_sha256Edit->setText(tr("Calculating..."));
    m_sha512Edit->setText(tr("Calculating..."));
    m_hashResultContainer->show();

    const QString filePath = m_hashedFilePath;
    AsyncTask::run<Core::Result<Core::FileUtilsBackend::FileHashes>>(
        this,
        [filePath](AsyncTask::CancelCheck) {
            return Core::FileUtilsBackend::computeHashes(filePath);
        },
        [this, filePath](Core::Result<Core::FileUtilsBackend::FileHashes> result) {
            if (filePath != m_hashedFilePath) return;
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            if (result.hasValue()) {
                const auto& hashes = result.value();
                m_md5Edit->setText(hashes.md5);
                m_sha1Edit->setText(hashes.sha1);
                m_sha256Edit->setText(hashes.sha256);
                m_sha512Edit->setText(hashes.sha512);
                checkHashMatch();
            } else {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(result.error().toUserFriendlyMessage()));
                m_hashDropZone->show();
                m_hashResultContainer->hide();
            }
        });
}

void FilesPage::checkHashMatch() {
    QString expected = m_verifyHashInput->text().trimmed().toLower();
    if (expected.isEmpty()) {
        m_verifyResultLabel->clear();
        return;
    }

    QStringList calculated = {
        m_md5Edit->text().toLower(),
        m_sha1Edit->text().toLower(),
        m_sha256Edit->text().toLower(),
        m_sha512Edit->text().toLower()
    };

    if (calculated.contains(expected)) {
        m_verifyResultLabel->setText(tr("✓ Matched"));
        m_verifyResultLabel->setStyleSheet("color: #10b981; font-weight: bold; padding-left: 10px;");
    } else {
        m_verifyResultLabel->setText(tr("✗ Does Not Match"));
        m_verifyResultLabel->setStyleSheet("color: #ef4444; font-weight: bold; padding-left: 10px;");
    }
}

void FilesPage::resetWorkspace() {
    m_hashedFilePath.clear();
    m_verifyHashInput->clear();
    m_verifyResultLabel->clear();
    m_hashResultContainer->hide();
    m_hashDropZone->show();
    m_pageStack->setCurrentIndex(0);
}

void FilesPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void FilesPage::retranslateUi() {
    if (m_hashTabBtn) m_hashTabBtn->setText(tr("Calculate Hash"));
    if (m_tabDescriptionLabel) {
        m_tabDescriptionLabel->setText(tr("Calculate and verify MD5, SHA-1, SHA-256, and SHA-512 checksum values of any file."));
    }
    if (m_hashDropZone) {
        m_hashDropZone->setInstruction(tr("Drop File Here to Calculate Hash"), tr("or click to select file"));
    }
}

} // namespace LocalTools::UI
