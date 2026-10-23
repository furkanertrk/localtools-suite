#ifndef LOCALTOOLS_CORE_OPERATIONS_TRIM_AUDIO_H
#define LOCALTOOLS_CORE_OPERATIONS_TRIM_AUDIO_H

#include "../tool_operation.h"

namespace LocalTools::Core {

class TrimAudioOperation : public IToolOperation {
public:
    TrimAudioOperation();
    ~TrimAudioOperation() override = default;

    QString displayName() const override { return "Trim Audio"; }
    
    void setInputFile(const QString& file);
    void setOutputDirectory(const QString& dir);
    void setTrimRange(double startSec, double endSec);

    // IToolOperation interface
    Result<void> execute() override;

private:
    QString m_inputFile;
    QString m_outputDir;
    double m_startSec{0.0};
    double m_endSec{0.0};
    bool m_isCancelled{false};
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_OPERATIONS_TRIM_AUDIO_H
