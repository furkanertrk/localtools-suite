#include "output_file_dialog.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>

#include "../../services/file_service.h"
#include "../../services/settings_service.h"

namespace LocalTools::UI::OutputFileDialog {

bool refersToProtectedInput(const QString& outputPath, const QStringList& inputPaths) {
    if (outputPath.trimmed().isEmpty()) return false;

    for (const QString& inputPath : inputPaths) {
        if (Services::FileService::instance().pathsReferToSameFile(outputPath, inputPath)) {
            return true;
        }
    }
    return false;
}

QString getSaveFileName(QWidget* parent,
                        const QString& caption,
                        const QString& suggestedPath,
                        const QString& filter,
                        const QStringList& protectedInputPaths) {
    QFileDialog::Options options;
    if (!Services::SettingsService::instance().getConfirmOverwrite()) {
        options |= QFileDialog::DontConfirmOverwrite;
    }

    QString nextSuggestedPath = suggestedPath;
    while (true) {
        const QString selectedPath = QFileDialog::getSaveFileName(
            parent, caption, nextSuggestedPath, filter, nullptr, options);
        if (selectedPath.isEmpty()) return {};

        if (!refersToProtectedInput(selectedPath, protectedInputPaths)) {
            return selectedPath;
        }

        QMessageBox::warning(
            parent,
            QCoreApplication::translate("OutputFileDialog", "Choose a different output file"),
            QCoreApplication::translate(
                "OutputFileDialog",
                "The output file cannot replace one of the source files. Choose a different name or folder."));
        nextSuggestedPath = selectedPath;
    }
}

} // namespace LocalTools::UI::OutputFileDialog
