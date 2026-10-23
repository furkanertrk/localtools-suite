#include "native_vision_backend.h"

#include <QColor>
#include <QPainter>
#include <QTransform>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace LocalTools::Core {

IdCardAnalysisResult NativeVisionBackend::analyzeIdCard(const QImage& img) {
    IdCardAnalysisResult res;

    if (img.isNull()) {
        res.isValid = false;
        res.errorMessage = "Image file could not be opened or is empty.";
        return res;
    }

    if (img.width() < 180 || img.height() < 180) {
        res.isValid = false;
        res.errorMessage = "Image resolution is too low for ID detection (must be at least 180x180 px).";
        return res;
    }

    // 1. Scale down to a normalized working resolution for noise-resistant & fast processing
    int targetMaxDim = 360;
    QImage workImg = img.scaled(targetMaxDim, targetMaxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int w = workImg.width();
    int h = workImg.height();

    double scaleX = static_cast<double>(img.width()) / w;
    double scaleY = static_cast<double>(img.height()) / h;

    // 2. Compute Grayscale and Sobel Edge Gradient Magnitude
    std::vector<uint8_t> gray(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            gray[y * w + x] = static_cast<uint8_t>(qGray(workImg.pixel(x, y)));
        }
    }

    std::vector<uint8_t> edges(w * h, 0);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int gx = -gray[(y - 1) * w + (x - 1)] + gray[(y - 1) * w + (x + 1)]
                     - 2 * gray[y * w + (x - 1)] + 2 * gray[y * w + (x + 1)]
                     - gray[(y + 1) * w + (x - 1)] + gray[(y + 1) * w + (x + 1)];

            int gy = -gray[(y - 1) * w + (x - 1)] - 2 * gray[(y - 1) * w + x] - gray[(y - 1) * w + (x + 1)]
                     + gray[(y + 1) * w + (x - 1)] + 2 * gray[(y + 1) * w + x] + gray[(y + 1) * w + (x + 1)];

            int mag = (std::abs(gx) + std::abs(gy)) / 4;
            edges[y * w + x] = static_cast<uint8_t>(std::min(255, mag));
        }
    }

    // 3. Background Color Estimation from Perimeter (outer 6% border band)
    int borderBandX = std::max(2, static_cast<int>(w * 0.06));
    int borderBandY = std::max(2, static_cast<int>(h * 0.06));

    double sumR = 0, sumG = 0, sumB = 0;
    int sampleCount = 0;
    std::vector<double> borderGrays;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (x < borderBandX || x >= w - borderBandX || y < borderBandY || y >= h - borderBandY) {
                QRgb p = workImg.pixel(x, y);
                sumR += qRed(p);
                sumG += qGreen(p);
                sumB += qBlue(p);
                borderGrays.push_back(qGray(p));
                sampleCount++;
            }
        }
    }

    double bgR = sumR / std::max(1, sampleCount);
    double bgG = sumG / std::max(1, sampleCount);
    double bgB = sumB / std::max(1, sampleCount);

    // Compute standard deviation of background color
    double sumSqDiff = 0;
    for (double gVal : borderGrays) {
        sumSqDiff += std::pow(gVal - qGray(qRgb(bgR, bgG, bgB)), 2);
    }
    double bgStdDev = std::sqrt(sumSqDiff / std::max(1, sampleCount));
    double colorDiffThreshold = std::clamp(bgStdDev * 2.2, 22.0, 50.0);

    // 4. Foreground Card Mask Detection
    std::vector<uint8_t> mask(w * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb p = workImg.pixel(x, y);
            double dist = std::sqrt(std::pow(qRed(p) - bgR, 2) +
                                    std::pow(qGreen(p) - bgG, 2) +
                                    std::pow(qBlue(p) - bgB, 2));

            uint8_t edgeVal = edges[y * w + x];
            if (dist > colorDiffThreshold || edgeVal > 28) {
                mask[y * w + x] = 1;
            }
        }
    }

    // 5. Morphological Closing (Dilation 3x3 then Erosion 3x3) to connect card elements
    std::vector<uint8_t> dilated(w * h, 0);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            uint8_t val = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (mask[(y + dy) * w + (x + dx)] == 1) {
                        val = 1;
                        break;
                    }
                }
                if (val == 1) break;
            }
            dilated[y * w + x] = val;
        }
    }

    std::vector<uint8_t> closed(w * h, 0);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            uint8_t val = 1;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dilated[(y + dy) * w + (x + dx)] == 0) {
                        val = 0;
                        break;
                    }
                }
                if (val == 0) break;
            }
            closed[y * w + x] = val;
        }
    }

    // 6. Find Card Boundaries via Row and Column Projections
    std::vector<int> rowProjections(h, 0);
    std::vector<int> colProjections(w, 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (closed[y * w + x] == 1) {
                rowProjections[y]++;
                colProjections[x]++;
            }
        }
    }

    int cropTop = 0;
    int cropBottom = h - 1;
    int cropLeft = 0;
    int cropRight = w - 1;

    int minCardThicknessX = static_cast<int>(w * 0.16);
    int minCardThicknessY = static_cast<int>(h * 0.16);

    // Scan Top
    for (int y = 0; y < h / 2; ++y) {
        if (rowProjections[y] > minCardThicknessX) {
            cropTop = y;
            break;
        }
    }
    // Scan Bottom
    for (int y = h - 1; y > h / 2; --y) {
        if (rowProjections[y] > minCardThicknessX) {
            cropBottom = y;
            break;
        }
    }
    // Scan Left
    for (int x = 0; x < w / 2; ++x) {
        if (colProjections[x] > minCardThicknessY) {
            cropLeft = x;
            break;
        }
    }
    // Scan Right
    for (int x = w - 1; x > w / 2; --x) {
        if (colProjections[x] > minCardThicknessY) {
            cropRight = x;
            break;
        }
    }

    // If borders are not detected (e.g. full frame image), fallback to full image
    if (cropRight <= cropLeft || cropBottom <= cropTop) {
        cropLeft = 0;
        cropTop = 0;
        cropRight = w - 1;
        cropBottom = h - 1;
    }

    // Add safe padding (2-3px at low-res) to preserve rounded card corners
    cropLeft = std::max(0, cropLeft - 2);
    cropTop = std::max(0, cropTop - 2);
    cropRight = std::min(w - 1, cropRight + 2);
    cropBottom = std::min(h - 1, cropBottom + 2);

    // Map back to original image coordinates
    int fullLeft = std::clamp(static_cast<int>(cropLeft * scaleX), 0, img.width() - 1);
    int fullTop = std::clamp(static_cast<int>(cropTop * scaleY), 0, img.height() - 1);
    int fullRight = std::clamp(static_cast<int>(cropRight * scaleX), 0, img.width() - 1);
    int fullBottom = std::clamp(static_cast<int>(cropBottom * scaleY), 0, img.height() - 1);

    int fullCardW = fullRight - fullLeft + 1;
    int fullCardH = fullBottom - fullTop + 1;

    res.cropRect = QRect(fullLeft, fullTop, fullCardW, fullCardH);
    res.aspectRatio = static_cast<double>(fullCardW) / std::max(1, fullCardH);
    res.isPortrait = (res.aspectRatio < 1.0);
    res.areaPercentage = (static_cast<double>(fullCardW * fullCardH) / (img.width() * img.height())) * 100.0;

    // 7. Calculate Edge Density inside the detected card area
    int cardEdgeCount = 0;
    int cardPixels = (cropRight - cropLeft + 1) * (cropBottom - cropTop + 1);
    for (int y = cropTop; y <= cropBottom; ++y) {
        for (int x = cropLeft; x <= cropRight; ++x) {
            if (edges[y * w + x] > 32) {
                cardEdgeCount++;
            }
        }
    }
    res.edgeDensity = (static_cast<double>(cardEdgeCount) / std::max(1, cardPixels)) * 100.0;

    // 8. Strict Validation Rules for ID Cards (ISO/IEC 7810 ID-1 Standard)
    double effectiveRatio = res.isPortrait ? (1.0 / res.aspectRatio) : res.aspectRatio;

    // Rule A: Area Fraction Check
    if (res.areaPercentage < 10.0) {
        res.isValid = false;
        res.errorMessage = "Could not detect ID card in image (document is too small or blends into background).";
        return res;
    }

    // Rule B: Standard ID-1 Aspect Ratio Check (Target: 85.60 mm / 53.98 mm ≈ 1.586)
    // Tolerated range: 1.34 to 1.84 (covers ID card, Driving License, Residence Permit, Passport card)
    if (effectiveRatio < 1.34 || effectiveRatio > 1.84) {
        res.isValid = false;
        res.errorMessage = QString("Aspect ratio (%1) does not match standard ID card dimensions (ISO/IEC 7810 ID-1, ~1.59). Please upload a valid ID card or driver license photo.")
                               .arg(effectiveRatio, 0, 'f', 2);
        return res;
    }

    // Rule C: Edge & Detail Density Check (ID cards have text, numbers, MRZ, chip, photo)
    if (res.edgeDensity < 2.0) {
        res.isValid = false;
        res.errorMessage = "No text or security features recognized on ID card (image is blank or too blurry).";
        return res;
    }

    if (res.edgeDensity > 50.0) {
        res.isValid = false;
        res.errorMessage = "Image structure does not conform to ID card standards (too noisy or textured).";
        return res;
    }

    // All validation passed
    res.isValid = true;
    return res;
}

bool NativeVisionBackend::isValidIdCard(const QImage& img, QString* outErrorMessage) {
    IdCardAnalysisResult analysis = analyzeIdCard(img);
    if (!analysis.isValid && outErrorMessage) {
        *outErrorMessage = analysis.errorMessage;
    }
    return analysis.isValid;
}

QImage NativeVisionBackend::autoCropAndCorrectPerspective(const QImage& img) {
    if (img.isNull()) return img;

    IdCardAnalysisResult analysis = analyzeIdCard(img);

    // If valid and a cropped rectangle is detected
    if (analysis.cropRect.isValid() && analysis.cropRect.width() > 50 && analysis.cropRect.height() > 50) {
        QImage cropped = img.copy(analysis.cropRect);

        // Normalize orientation: If photo was uploaded in portrait, rotate to landscape
        if (analysis.isPortrait) {
            QTransform rotation;
            rotation.rotate(90);
            cropped = cropped.transformed(rotation, Qt::SmoothTransformation);
        }

        return cropped;
    }

    return img;
}

} // namespace LocalTools::Core
