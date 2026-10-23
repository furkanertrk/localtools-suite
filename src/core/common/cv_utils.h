#ifndef LOCALTOOLS_CORE_COMMON_CV_UTILS_H
#define LOCALTOOLS_CORE_COMMON_CV_UTILS_H

#include <QImage>
#include <QDebug>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace LocalTools::Core {

class CvUtils {
public:
    static cv::Mat QImageToCvMat(const QImage& inImage, bool inCloneImageData = true) {
        switch (inImage.format()) {
            // 8-bit, 4 channel
            case QImage::Format_ARGB32:
            case QImage::Format_ARGB32_Premultiplied: {
                cv::Mat mat(inImage.height(), inImage.width(),
                            CV_8UC4,
                            const_cast<uchar*>(inImage.bits()),
                            static_cast<size_t>(inImage.bytesPerLine()));
                return (inCloneImageData ? mat.clone() : mat);
            }
            // 8-bit, 3 channel
            case QImage::Format_RGB32:
            case QImage::Format_RGB888: {
                QImage swapped = inImage;
                if (inImage.format() == QImage::Format_RGB32) {
                    swapped = swapped.convertToFormat(QImage::Format_RGB888);
                }
                swapped = swapped.rgbSwapped();
                return cv::Mat(swapped.height(), swapped.width(),
                               CV_8UC3,
                               const_cast<uchar*>(swapped.bits()),
                               static_cast<size_t>(swapped.bytesPerLine())).clone();
            }
            // 8-bit, 1 channel
            case QImage::Format_Grayscale8:
            case QImage::Format_Indexed8: {
                cv::Mat mat(inImage.height(), inImage.width(),
                            CV_8UC1,
                            const_cast<uchar*>(inImage.bits()),
                            static_cast<size_t>(inImage.bytesPerLine()));
                return (inCloneImageData ? mat.clone() : mat);
            }
            default:
                qWarning() << "CvUtils::QImageToCvMat() - QImage format not handled in switch:" << inImage.format();
                break;
        }
        return cv::Mat();
    }

    static QImage CvMatToQImage(const cv::Mat& inMat) {
        switch (inMat.type()) {
            // 8-bit, 4 channel
            case CV_8UC4: {
                QImage image(inMat.data,
                             inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_ARGB32);
                return image.copy();
            }
            // 8-bit, 3 channel
            case CV_8UC3: {
                QImage image(inMat.data,
                             inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_RGB888);
                return image.rgbSwapped().copy();
            }
            // 8-bit, 1 channel
            case CV_8UC1: {
                QImage image(inMat.data,
                             inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_Grayscale8);
                return image.copy();
            }
            default:
                qWarning() << "CvUtils::CvMatToQImage() - cv::Mat type not handled in switch:" << inMat.type();
                break;
        }
        return QImage();
    }
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_COMMON_CV_UTILS_H
