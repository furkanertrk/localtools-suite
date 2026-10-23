#ifndef LOCALTOOLS_CORE_BACKENDS_NATIVE_VISION_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_NATIVE_VISION_BACKEND_H

#include <QImage>
#include <QRect>
#include <QString>

namespace LocalTools::Core {

struct IdCardAnalysisResult {
    bool isValid = false;
    QString errorMessage;
    QRect cropRect;
    double aspectRatio = 0.0;
    double areaPercentage = 0.0;
    double edgeDensity = 0.0;
    bool isPortrait = false;
};

class NativeVisionBackend {
public:
    /**
     * @brief Performs comprehensive analysis on the image to verify if it is an ID card.
     * Checks aspect ratio, edge density, text/contrast distribution, and background boundaries.
     * @param img Input image to check.
     * @param outErrorMessage Optional pointer to receive a user-friendly error message if invalid.
     * @return true if the image is confirmed to be a valid ID card.
     */
    static bool isValidIdCard(const QImage& img, QString* outErrorMessage = nullptr);

    /**
     * @brief Detects ID card boundaries on background, removes surrounding background,
     * normalizes orientation to landscape, and returns a clean, tightly cropped ID card image.
     * @param img Input image.
     * @return Cleaned and cropped ID card image.
     */
    static QImage autoCropAndCorrectPerspective(const QImage& img);

    /**
     * @brief Detailed computer vision analysis of the ID card.
     */
    static IdCardAnalysisResult analyzeIdCard(const QImage& img);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_NATIVE_VISION_BACKEND_H
