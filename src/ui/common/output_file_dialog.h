#ifndef LOCALTOOLS_UI_COMMON_OUTPUT_FILE_DIALOG_H
#define LOCALTOOLS_UI_COMMON_OUTPUT_FILE_DIALOG_H

#include <QString>
#include <QStringList>

class QWidget;

namespace LocalTools::UI::OutputFileDialog {

bool refersToProtectedInput(const QString& outputPath, const QStringList& inputPaths);

QString getSaveFileName(QWidget* parent,
                        const QString& caption,
                        const QString& suggestedPath,
                        const QString& filter,
                        const QStringList& protectedInputPaths = {});

} // namespace LocalTools::UI::OutputFileDialog

#endif // LOCALTOOLS_UI_COMMON_OUTPUT_FILE_DIALOG_H
