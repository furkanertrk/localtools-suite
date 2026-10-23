#ifndef LOCALTOOLS_CORE_OPERATIONS_COMPRESS_AUDIO_H
#define LOCALTOOLS_CORE_OPERATIONS_COMPRESS_AUDIO_H

#include "../tool_operation.h"
#include <QStringList>

namespace LocalTools::Core {

class CompressAudioOperation : public IToolOperation {
public:
    CompressAudioOperation();
    ~CompressAudioOperation() override = default;

    QString displayName() const override { return "Compress Audio"; }
    
    void setInputFiles(const QStringList& files);
    void setOutputDirectory(const QString& dir);
    void setTargetBitrate(int kbps); // e.g. 64, 96, 128

    // IToolOperation interface
    Result<void> execute() override;

private:
    QStringList m_inputFiles;
    QString m_outputDir;
    int m_targetBitrateKbps{96};
    bool m_isCancelled{false};
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_OPERATIONS_COMPRESS_AUDIO_H
