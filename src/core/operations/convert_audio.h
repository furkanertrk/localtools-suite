#ifndef LOCALTOOLS_CORE_OPERATIONS_CONVERT_AUDIO_H
#define LOCALTOOLS_CORE_OPERATIONS_CONVERT_AUDIO_H

#include "../tool_operation.h"
#include <QStringList>

namespace LocalTools::Core {

class ConvertAudioOperation : public IToolOperation {
public:
    ConvertAudioOperation();
    ~ConvertAudioOperation() override = default;

    QString displayName() const override { return "Convert Audio Format"; }
    
    // Setters for operation state
    void setInputFiles(const QStringList& files);
    void setOutputDirectory(const QString& dir);
    void setTargetFormat(const QString& format); // mp3, wav, flac, etc.
    void setTargetBitrate(int kbps);
    void setStripMetadata(bool strip);

    // IToolOperation interface
    Result<void> execute() override;

private:
    QStringList m_inputFiles;
    QString m_outputDir;
    QString m_targetFormat{"mp3"};
    int m_targetBitrateKbps{192};
    bool m_stripMetadata{false};
    bool m_isCancelled{false};
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_OPERATIONS_CONVERT_AUDIO_H
