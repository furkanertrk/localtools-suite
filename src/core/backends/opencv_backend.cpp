#include "opencv_backend.h"
#include "../common/cv_utils.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <vector>
#include <algorithm>
#include <iostream>

namespace LocalTools::Core {

static std::vector<cv::Point> findLargestQuadrilateral(const cv::Mat& grayMat) {
    cv::Mat blurred, edged;
    cv::GaussianBlur(grayMat, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edged, 50, 150);

    // Optional: Dilate to close gaps
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::dilate(edged, edged, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edged, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Sort contours by area descending
    std::sort(contours.begin(), contours.end(), [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
        return cv::contourArea(a) > cv::contourArea(b);
    });

    for (const auto& contour : contours) {
        double peri = cv::arcLength(contour, true);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, 0.02 * peri, true);

        if (approx.size() == 4) {
            double area = cv::contourArea(approx);
            double imageArea = grayMat.cols * grayMat.rows;
            // The card should take up a reasonable portion of the image (e.g., at least 5%)
            if (area > imageArea * 0.05) {
                return approx;
            }
        }
    }
    return {};
}

static void orderPoints(const std::vector<cv::Point>& src, std::vector<cv::Point2f>& dst) {
    // Orders points: top-left, top-right, bottom-right, bottom-left
    dst.resize(4);
    
    // Sum and difference
    std::vector<int> sum(4), diff(4);
    for (int i = 0; i < 4; ++i) {
        sum[i] = src[i].x + src[i].y;
        diff[i] = src[i].y - src[i].x;
    }

    auto minMaxSum = std::minmax_element(sum.begin(), sum.end());
    auto minMaxDiff = std::minmax_element(diff.begin(), diff.end());

    dst[0] = src[std::distance(sum.begin(), minMaxSum.first)];    // top-left has minimum sum
    dst[2] = src[std::distance(sum.begin(), minMaxSum.second)];   // bottom-right has maximum sum
    dst[1] = src[std::distance(diff.begin(), minMaxDiff.first)];  // top-right has minimum diff
    dst[3] = src[std::distance(diff.begin(), minMaxDiff.second)]; // bottom-left has maximum diff
}

bool OpenCVBackend::isValidIdCard(const QImage& img) {
    if (img.isNull()) return false;
    
    cv::Mat mat = CvUtils::QImageToCvMat(img);
    if (mat.empty()) return false;
    
    cv::Mat gray;
    if (mat.channels() == 3 || mat.channels() == 4) {
        cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = mat;
    }

    auto quad = findLargestQuadrilateral(gray);
    return !quad.empty();
}

QImage OpenCVBackend::autoCropAndCorrectPerspective(const QImage& img) {
    if (img.isNull()) return img;

    cv::Mat mat = CvUtils::QImageToCvMat(img);
    if (mat.empty()) return img;

    cv::Mat gray;
    if (mat.channels() == 3 || mat.channels() == 4) {
        cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = mat;
    }

    auto quad = findLargestQuadrilateral(gray);
    if (quad.empty()) {
        std::cerr << "[OpenCVBackend] Failed to find a valid quadrilateral for ID card." << std::endl;
        return img; // Fallback to original image if detection fails
    }

    std::vector<cv::Point2f> srcPoints;
    orderPoints(quad, srcPoints);

    // Calculate dimensions of the new image
    double widthA = std::sqrt(std::pow(srcPoints[2].x - srcPoints[3].x, 2) + std::pow(srcPoints[2].y - srcPoints[3].y, 2));
    double widthB = std::sqrt(std::pow(srcPoints[1].x - srcPoints[0].x, 2) + std::pow(srcPoints[1].y - srcPoints[0].y, 2));
    int maxWidth = static_cast<int>(std::max(widthA, widthB));

    double heightA = std::sqrt(std::pow(srcPoints[1].x - srcPoints[2].x, 2) + std::pow(srcPoints[1].y - srcPoints[2].y, 2));
    double heightB = std::sqrt(std::pow(srcPoints[0].x - srcPoints[3].x, 2) + std::pow(srcPoints[0].y - srcPoints[3].y, 2));
    int maxHeight = static_cast<int>(std::max(heightA, heightB));

    // Force standard ID ratio (85.60 / 53.98) if the aspect ratio is wildly off due to distortion
    double standardRatio = 85.60 / 53.98;
    double currentRatio = static_cast<double>(maxWidth) / maxHeight;
    
    // Correct extreme distortions
    if (currentRatio < 1.3 || currentRatio > 1.8) {
        if (maxWidth > maxHeight) {
            maxHeight = static_cast<int>(maxWidth / standardRatio);
        } else {
            maxWidth = static_cast<int>(maxHeight * standardRatio);
        }
    }

    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(0, 0),
        cv::Point2f(maxWidth - 1, 0),
        cv::Point2f(maxWidth - 1, maxHeight - 1),
        cv::Point2f(0, maxHeight - 1)
    };

    cv::Mat transformMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    cv::Mat warped;
    cv::warpPerspective(mat, warped, transformMatrix, cv::Size(maxWidth, maxHeight));

    return CvUtils::CvMatToQImage(warped);
}

} // namespace LocalTools::Core
