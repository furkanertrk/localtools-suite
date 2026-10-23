#ifndef LOCALTOOLS_CORE_BACKENDS_OPENCV_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_OPENCV_BACKEND_H

#include <QImage>

namespace LocalTools::Core {

class OpenCVBackend {
public:
    /**
     * @brief Checks if the image contains a valid ID card using contour/edge analysis.
     * @param img The input image (e.g., QImage from UI).
     * @return true if a valid ID card structure is detected.
     */
    static bool isValidIdCard(const QImage& img);

    /**
     * @brief Automatically finds the ID card in the image, crops it, and corrects perspective.
     * @param img The input image containing the ID card on a background.
     * @return A tightly cropped and perspective-corrected QImage of the ID card.
     *         If detection fails, returns the original image (or a basic cropped version).
     */
    static QImage autoCropAndCorrectPerspective(const QImage& img);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_OPENCV_BACKEND_H
