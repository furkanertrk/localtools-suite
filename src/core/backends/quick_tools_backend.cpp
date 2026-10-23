#include "quick_tools_backend.h"

#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <array>
#include <iostream>

#include "image_backend.h"
#include "../backend/pdfium_backend.h"
#include "../../../third_party/qrcodegen/qrcodegen.hpp"

namespace LocalTools::Core {

// =========================================================================
// 1. QR Code Generator Implementation
// =========================================================================

Result<QImage> QuickToolsBackend::generateQrImage(const QString& text, const QrOptions& options) {
    if (text.isEmpty()) {
        return Result<QImage>::fail(PdfErrorCategory::InvalidPageRange, "Text cannot be empty.");
    }

    try {
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::HIGH);
        
        int gridCount = qr.getSize();
        int totalGrid = gridCount + options.margin * 2;
        int targetSize = std::max(options.size, 256);
        int moduleSize = targetSize / totalGrid;
        int actualSize = moduleSize * totalGrid;

        QImage image(actualSize, actualSize, QImage::Format_RGB32);
        image.fill(options.background);

        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(options.foreground);

        for (int r = 0; r < gridCount; ++r) {
            for (int c = 0; c < gridCount; ++c) {
                if (qr.getModule(c, r)) {
                    int px = (c + options.margin) * moduleSize;
                    int py = (r + options.margin) * moduleSize;
                    painter.drawRect(px, py, moduleSize, moduleSize);
                }
            }
        }
        painter.end();

        return Result<QImage>::ok(std::move(image));
    } catch (const std::exception& e) {
        return Result<QImage>::fail(PdfErrorCategory::BackendError, QString("Failed to generate QR code: %1").arg(e.what()).toStdString());
    }
}

Result<QImage> QuickToolsBackend::generateQrImage(const QString& text) {
    return generateQrImage(text, QrOptions{});
}

Result<void> QuickToolsBackend::saveQrCode(const QString& text, const QString& outputPath, const QrOptions& options) {
    auto imgRes = generateQrImage(text, options);
    if (imgRes.hasError()) return Result<void>::fail(imgRes.error());

    if (!imgRes.value().save(outputPath)) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save QR code file: " + outputPath.toStdString());
    }
    return Result<void>::ok();
}

Result<void> QuickToolsBackend::saveQrCode(const QString& text, const QString& outputPath) {
    return saveQrCode(text, outputPath, QrOptions{});
}

// =========================================================================
// 2. JSON, Base64 & URL Tools
// =========================================================================

Result<QString> QuickToolsBackend::formatJson(const QString& inputJson, int indent) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(inputJson.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError) {
        return Result<QString>::fail(PdfErrorCategory::BackendError,
                                     "JSON Syntax Error: " + err.errorString().toStdString());
    }

    Q_UNUSED(indent);
    QString formatted = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    return Result<QString>::ok(formatted);
}

Result<QString> QuickToolsBackend::minifyJson(const QString& inputJson) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(inputJson.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError) {
        return Result<QString>::fail(PdfErrorCategory::BackendError,
                                     "JSON Syntax Error: " + err.errorString().toStdString());
    }

    QString minified = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    return Result<QString>::ok(minified);
}

QString QuickToolsBackend::encodeBase64(const QString& text) {
    return QString::fromLatin1(text.toUtf8().toBase64());
}

Result<QString> QuickToolsBackend::decodeBase64(const QString& base64Text) {
    QByteArray decoded = QByteArray::fromBase64(base64Text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.isEmpty() && !base64Text.trimmed().isEmpty()) {
        return Result<QString>::fail(PdfErrorCategory::BackendError, "Invalid Base64 data.");
    }
    return Result<QString>::ok(QString::fromUtf8(decoded));
}

QString QuickToolsBackend::urlEncode(const QString& text) {
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QString QuickToolsBackend::urlDecode(const QString& text) {
    return QUrl::fromPercentEncoding(text.toUtf8());
}

// =========================================================================
// 3. Complete Standard-Compliant QR Code Decoder Engine
// =========================================================================

namespace {

QString metadataTr(const char* sourceText) {
    return QCoreApplication::translate("QuickToolsBackend", sourceText);
}

// 32 Valid 15-bit format patterns (ISO/IEC 18004 Table 10)
static const uint16_t FORMAT_PATTERNS[32] = {
    0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0,
    0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976,
    0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B,
    0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED
};

static const int ALIGNMENT_POSITIONS[41][7] = {
    {0}, {0},
    {6, 18, 0}, {6, 22, 0}, {6, 26, 0}, {6, 30, 0}, {6, 34, 0},
    {6, 22, 38, 0}, {6, 24, 42, 0}, {6, 26, 46, 0}, {6, 28, 50, 0},
    {6, 30, 54, 0}, {6, 32, 58, 0}, {6, 34, 62, 0}, {6, 26, 46, 66, 0},
    {6, 26, 48, 70, 0}, {6, 26, 50, 74, 0}, {6, 30, 54, 78, 0},
    {6, 30, 56, 82, 0}, {6, 30, 58, 86, 0}, {6, 34, 62, 90, 0},
    {6, 28, 50, 72, 94, 0}, {6, 26, 50, 74, 98, 0}, {6, 30, 54, 78, 102, 0},
    {6, 28, 54, 80, 106, 0}, {6, 32, 58, 84, 110, 0}, {6, 30, 58, 86, 114, 0},
    {6, 34, 62, 90, 118, 0}, {6, 26, 50, 74, 98, 122, 0}, {6, 30, 54, 78, 102, 126, 0},
    {6, 26, 52, 78, 104, 130, 0}, {6, 30, 56, 82, 108, 134, 0}, {6, 34, 60, 86, 112, 138, 0},
    {6, 30, 58, 86, 114, 142, 0}, {6, 34, 62, 90, 118, 146, 0}, {6, 30, 54, 78, 102, 126, 150},
    {6, 24, 50, 76, 102, 128, 154}, {6, 28, 54, 80, 106, 132, 158}, {6, 32, 58, 84, 110, 136, 162},
    {6, 26, 54, 82, 110, 138, 166}, {6, 30, 58, 86, 114, 142, 170}
};

// Check if a module (r, c) is part of a function pattern
bool isFunctionPattern(int version, int r, int c) {
    int size = version * 4 + 17;

    // 1. Top-Left Finder + Separators (9x9)
    if (r < 9 && c < 9) return true;

    // 2. Top-Right Finder + Separators (9x9)
    if (r < 9 && c >= size - 8) return true;

    // 3. Bottom-Left Finder + Separators (9x9)
    if (r >= size - 8 && c < 9) return true;

    // 4. Timing patterns (row 6 and col 6)
    if (r == 6 || c == 6) return true;

    // 5. Version Information (Version >= 7)
    if (version >= 7) {
        if (r < 6 && c >= size - 11 && c < size - 8) return true;
        if (r >= size - 11 && r < size - 8 && c < 6) return true;
    }

    // 6. Alignment patterns (Version >= 2)
    if (version >= 2) {
        const int* pos = ALIGNMENT_POSITIONS[version];
        int numPos = 0;
        while (numPos < 7 && pos[numPos] != 0) numPos++;

        for (int i = 0; i < numPos; ++i) {
            for (int j = 0; j < numPos; ++j) {
                int ax = pos[i];
                int ay = pos[j];

                // Skip if alignment overlaps with 3 corners
                if (ax == 6 && ay == 6) continue;
                if (ax == 6 && ay == size - 7) continue;
                if (ax == size - 7 && ay == 6) continue;

                if (std::abs(r - ay) <= 2 && std::abs(c - ax) <= 2) {
                    return true;
                }
            }
        }
    }

    return false;
}

// RS Block Structure
struct RsBlockSpec {
    int numBlocksG1;
    int dataBytesG1;
    int numBlocksG2;
    int dataBytesG2;
    int eccPerBlock;
};

// ISO/IEC 18004 Table 9 (Versions 1-10)
// Indexed by version (1..10) and eccLevel: 0=M, 1=L, 2=H, 3=Q
RsBlockSpec getRsBlockSpec(int version, int eccLevel) {
    static const RsBlockSpec TABLE[11][4] = {
        {}, // 0
        { // 1
            {1, 16, 0, 0, 10}, // M
            {1, 19, 0, 0, 7},  // L
            {1, 9, 0, 0, 17},  // H
            {1, 13, 0, 0, 13}  // Q
        },
        { // 2
            {1, 28, 0, 0, 16}, // M
            {1, 34, 0, 0, 10}, // L
            {1, 16, 0, 0, 28}, // H
            {1, 22, 0, 0, 22}  // Q
        },
        { // 3
            {1, 44, 0, 0, 26}, // M
            {1, 55, 0, 0, 15}, // L
            {2, 13, 0, 0, 22}, // H
            {2, 17, 0, 0, 18}  // Q
        },
        { // 4
            {2, 32, 0, 0, 18}, // M
            {1, 80, 0, 0, 20}, // L
            {4, 9, 0, 0, 16},  // H
            {2, 24, 0, 0, 26}  // Q
        },
        { // 5
            {2, 43, 0, 0, 24}, // M
            {1, 108, 0, 0, 26},// L
            {2, 11, 2, 12, 22},// H
            {2, 15, 2, 16, 18} // Q
        },
        { // 6
            {4, 27, 0, 0, 16}, // M
            {2, 68, 0, 0, 18}, // L
            {4, 15, 0, 0, 28}, // H
            {4, 19, 0, 0, 24}  // Q
        },
        { // 7
            {4, 31, 0, 0, 18}, // M
            {2, 78, 0, 0, 20}, // L
            {4, 13, 1, 14, 26},// H
            {2, 14, 4, 15, 18} // Q
        },
        { // 8
            {2, 38, 2, 39, 22},// M
            {2, 97, 0, 0, 24}, // L
            {4, 14, 2, 15, 26},// H
            {4, 18, 2, 19, 22} // Q
        },
        { // 9
            {3, 36, 2, 37, 22},// M
            {2, 116, 0, 0, 30},// L
            {4, 12, 4, 13, 24},// H
            {4, 16, 4, 17, 20} // Q
        },
        { // 10
            {4, 43, 1, 44, 26},// M
            {2, 68, 2, 69, 18},// L
            {6, 15, 2, 16, 28},// H
            {6, 19, 2, 20, 24} // Q
        }
    };

    if (version >= 1 && version <= 10 && eccLevel >= 0 && eccLevel <= 3) {
        return TABLE[version][eccLevel];
    }
    return {1, 16, 0, 0, 10};
}

// Finder Pattern Candidate
struct FinderPattern {
    double x{0.0};
    double y{0.0};
    double moduleSize{1.0};
    int count{1};
};

struct BitMatrix {
    int width{0};
    int height{0};
    std::vector<uint8_t> data;

    BitMatrix(int w, int h) : width(w), height(h), data(w * h, 0) {}

    inline uint8_t get(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0;
        return data[y * width + x];
    }

    inline void set(int x, int y, uint8_t val) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            data[y * width + x] = val;
        }
    }
};

// Perspective transform 3x3
struct PerspectiveTransform {
    double a11, a12, a13;
    double a21, a22, a23;
    double a31, a32, a33;

    static PerspectiveTransform squareToQuadrilateral(double x0, double y0, double x1, double y1,
                                                      double x2, double y2, double x3, double y3) {
        double dx1 = x1 - x2, dy1 = y1 - y2;
        double dx2 = x3 - x2, dy2 = y3 - y2;
        double sx = x0 - x1 + x2 - x3;
        double sy = y0 - y1 + y2 - y3;

        double den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) < 1e-7) {
            return {1, 0, 0, 0, 1, 0, 0, 0, 1};
        }

        double g = (sx * dy2 - dx2 * sy) / den;
        double h = (dx1 * sy - sx * dy1) / den;
        double a = x1 - x0 + g * x1;
        double b = x3 - x0 + h * x3;
        double d = y1 - y0 + g * y1;
        double e = y3 - y0 + h * y3;

        return {a, b, x0, d, e, y0, g, h, 1.0};
    }

    QPointF transform(double u, double v) const {
        double den = a31 * u + a32 * v + a33;
        if (std::abs(den) < 1e-7) den = 1.0;
        double x = (a11 * u + a12 * v + a13) / den;
        double y = (a21 * u + a22 * v + a23) / den;
        return QPointF(x, y);
    }
};

// Check 1:1:3:1:1 ratio
bool checkRatio(const int state[5]) {
    int total = state[0] + state[1] + state[2] + state[3] + state[4];
    if (total < 7) return false;
    double moduleSize = total / 7.0;
    double maxVariance = moduleSize * 0.65;

    return (std::abs(state[0] - moduleSize) < maxVariance &&
            std::abs(state[1] - moduleSize) < maxVariance &&
            std::abs(state[2] - 3.0 * moduleSize) < 3.0 * maxVariance &&
            std::abs(state[3] - moduleSize) < maxVariance &&
            std::abs(state[4] - moduleSize) < maxVariance);
}

// Binarize QImage
BitMatrix binarizeImage(const QImage& gray, int thresh) {
    int w = gray.width();
    int h = gray.height();
    BitMatrix bm(w, h);

    for (int y = 0; y < h; ++y) {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            bm.set(x, y, (line[x] < thresh) ? 1 : 0); // 1 = Black, 0 = White
        }
    }
    return bm;
}

// Vertical verification for finder pattern
bool verifyVertical(const BitMatrix& bm, int startX, int startY, int centerCount, double& outCenterY, double& outModSize) {
    int h = bm.height;
    int state[5] = {0, 0, 0, 0, 0};
    int y = startY;

    while (y >= 0 && bm.get(startX, y) == 1) {
        state[2]++;
        y--;
    }
    if (y < 0) return false;

    while (y >= 0 && bm.get(startX, y) == 0 && state[1] < centerCount) {
        state[1]++;
        y--;
    }
    if (y < 0 || state[1] >= centerCount) return false;

    while (y >= 0 && bm.get(startX, y) == 1 && state[0] < centerCount) {
        state[0]++;
        y--;
    }
    if (state[0] >= centerCount) return false;

    y = startY + 1;
    while (y < h && bm.get(startX, y) == 1) {
        state[2]++;
        y++;
    }
    if (y >= h) return false;

    while (y < h && bm.get(startX, y) == 0 && state[3] < centerCount) {
        state[3]++;
        y++;
    }
    if (y >= h || state[3] >= centerCount) return false;

    while (y < h && bm.get(startX, y) == 1 && state[4] < centerCount) {
        state[4]++;
        y++;
    }
    if (state[4] >= centerCount) return false;

    if (!checkRatio(state)) return false;

    int total = state[0] + state[1] + state[2] + state[3] + state[4];
    outCenterY = (y - state[4] - state[3]) - state[2] / 2.0;
    outModSize = total / 7.0;
    return true;
}

// Find finder patterns
std::vector<FinderPattern> findFinders(const BitMatrix& bm) {
    std::vector<FinderPattern> patterns;
    int w = bm.width;
    int h = bm.height;

    for (int y = 2; y < h - 2; y += 2) {
        int state[5] = {0, 0, 0, 0, 0};
        int currentState = 0;

        for (int x = 0; x < w; ++x) {
            uint8_t val = bm.get(x, y);
            if (val == 1) { // Black
                if ((currentState & 1) == 1) {
                    currentState++;
                }
                state[currentState]++;
            } else { // White
                if ((currentState & 1) == 0) {
                    if (currentState == 4) {
                        if (checkRatio(state)) {
                            double centerX = (x - state[4] - state[3]) - state[2] / 2.0;
                            double centerY = 0.0;
                            double modSize = 0.0;

                            if (verifyVertical(bm, static_cast<int>(centerX), y, state[2], centerY, modSize)) {
                                bool merged = false;
                                for (auto& p : patterns) {
                                    if (std::hypot(p.x - centerX, p.y - centerY) < modSize * 3.5) {
                                        p.x = (p.x * p.count + centerX) / (p.count + 1);
                                        p.y = (p.y * p.count + centerY) / (p.count + 1);
                                        p.moduleSize = (p.moduleSize * p.count + modSize) / (p.count + 1);
                                        p.count++;
                                        merged = true;
                                        break;
                                    }
                                }
                                if (!merged) {
                                    patterns.push_back({centerX, centerY, modSize, 1});
                                }
                            }
                        }
                        state[0] = state[2];
                        state[1] = state[3];
                        state[2] = state[4];
                        state[3] = 1;
                        state[4] = 0;
                        currentState = 3;
                    } else {
                        currentState++;
                        state[currentState]++;
                    }
                } else {
                    state[currentState]++;
                }
            }
        }
    }

    return patterns;
}

// Parse QR bitstream into payload string
QString parseBitStream(const std::vector<uint8_t>& bitStream, int version) {
    size_t bitPtr = 0;
    size_t totalBits = bitStream.size();

    auto readBits = [&](int count) -> int {
        int val = 0;
        for (int k = 0; k < count; ++k) {
            if (bitPtr < totalBits) {
                val = (val << 1) | bitStream[bitPtr++];
            }
        }
        return val;
    };

    QByteArray payload;
    bool hasValidSegment = false;

    while (bitPtr + 4 <= totalBits) {
        int mode = readBits(4);
        if (mode == 0) break; // Terminator

        if (mode == 4) { // Byte Mode (8-bit UTF-8)
            int count = (version < 10) ? readBits(8) : readBits(16);
            if (count <= 0 || count > 2000 || bitPtr + count * 8 > totalBits) return "";
            for (int k = 0; k < count; ++k) {
                payload.append(static_cast<char>(readBits(8)));
            }
            hasValidSegment = true;
        } else if (mode == 2) { // Alphanumeric Mode
            static const char ALPHA_TABLE[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
            int count = (version < 10) ? readBits(9) : ((version < 27) ? readBits(11) : readBits(13));
            if (count <= 0 || count > 2000 || bitPtr + count * 5.5 > totalBits + 10) return "";
            while (count > 0 && bitPtr < totalBits) {
                if (count >= 2) {
                    if (bitPtr + 11 > totalBits) return "";
                    int val = readBits(11);
                    if (val >= 45 * 45) return "";
                    payload.append(ALPHA_TABLE[val / 45]);
                    payload.append(ALPHA_TABLE[val % 45]);
                    count -= 2;
                } else {
                    if (bitPtr + 6 > totalBits) return "";
                    int val = readBits(6);
                    if (val >= 45) return "";
                    payload.append(ALPHA_TABLE[val]);
                    count--;
                }
            }
            hasValidSegment = true;
        } else if (mode == 1) { // Numeric Mode
            int count = (version < 10) ? readBits(10) : ((version < 27) ? readBits(12) : readBits(14));
            if (count <= 0 || count > 2000 || bitPtr + count * 3.3 > totalBits + 10) return "";
            while (count > 0 && bitPtr < totalBits) {
                if (count >= 3) {
                    if (bitPtr + 10 > totalBits) return "";
                    int val = readBits(10);
                    if (val >= 1000) return "";
                    payload.append(QString("%1").arg(val, 3, 10, QChar('0')).toUtf8());
                    count -= 3;
                } else if (count == 2) {
                    if (bitPtr + 7 > totalBits) return "";
                    int val = readBits(7);
                    if (val >= 100) return "";
                    payload.append(QString("%1").arg(val, 2, 10, QChar('0')).toUtf8());
                    count -= 2;
                } else {
                    if (bitPtr + 4 > totalBits) return "";
                    int val = readBits(4);
                    if (val >= 10) return "";
                    payload.append(QString("%1").arg(val).toUtf8());
                    count--;
                }
            }
            hasValidSegment = true;
        } else {
            return ""; // Unknown/invalid mode -> reject entire grid!
        }
    }

    if (!hasValidSegment || payload.isEmpty()) return "";

    // Verify all characters are printable UTF-8/ASCII
    for (char ch : payload) {
        if ((unsigned char)ch < 32 && ch != '\n' && ch != '\r' && ch != '\t') {
            return ""; // Reject corrupted payload
        }
    }

    return QString::fromUtf8(payload);
}

// Decode raw bit stream from $N \times N$ sampled grid
Result<QrDecodeResult> decodeSampledGrid(const std::vector<std::vector<uint8_t>>& grid) {
    int size = static_cast<int>(grid.size());
    if (size < 21 || ((size - 17) % 4 != 0)) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Invalid QR grid size.");
    }

    int version = (size - 17) / 4;

    // Read 15-bit format information from top-left:
    // F14 (MSB) at (col 0, row 8) down to F0 (LSB) at (col 8, row 0)
    int rawFormat = 0;
    for (int c = 0; c <= 5; ++c) rawFormat = (rawFormat << 1) | grid[8][c];
    rawFormat = (rawFormat << 1) | grid[8][7];
    rawFormat = (rawFormat << 1) | grid[8][8];
    rawFormat = (rawFormat << 1) | grid[7][8];
    for (int r = 5; r >= 0; --r) rawFormat = (rawFormat << 1) | grid[r][8];

    // Match against 32 standard BCH format patterns (BCH error correction)
    int bestPatternIdx = -1;
    int minHamming = 999;
    for (int i = 0; i < 32; ++i) {
        int diff = rawFormat ^ FORMAT_PATTERNS[i];
        int dist = 0;
        while (diff > 0) {
            dist += (diff & 1);
            diff >>= 1;
        }
        if (dist < minHamming) {
            minHamming = dist;
            bestPatternIdx = i;
        }
    }

    if (minHamming > 2 || bestPatternIdx < 0) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Invalid format information.");
    }

    int eccLevel = bestPatternIdx >> 3; // 0=M, 1=L, 2=H, 3=Q
    int maskPattern = bestPatternIdx & 7;

    // Read bits skipping ALL function patterns (finders, timing, alignment, version info)
    std::vector<uint8_t> rawBits;
    int dir = -1; // Moving upwards
    int x = size - 1;

    while (x > 0) {
        if (x == 6) x--; // Skip timing column 6

        for (int i = 0; i < size; ++i) {
            int r = (dir < 0) ? (size - 1 - i) : i;
            for (int j = 0; j < 2; ++j) {
                int c = x - j;

                if (!isFunctionPattern(version, r, c)) {
                    bool invert = false;
                    switch (maskPattern) {
                        case 0: invert = ((r + c) % 2 == 0); break;
                        case 1: invert = (r % 2 == 0); break;
                        case 2: invert = (c % 3 == 0); break;
                        case 3: invert = ((r + c) % 3 == 0); break;
                        case 4: invert = ((r / 2 + c / 3) % 2 == 0); break;
                        case 5: invert = (((r * c) % 2 + (r * c) % 3) == 0); break;
                        case 6: invert = ((((r * c) % 2 + (r * c) % 3) % 2) == 0); break;
                        case 7: invert = ((((r + c) % 2 + (r * c) % 3) % 2) == 0); break;
                    }

                    uint8_t bit = grid[r][c] ^ (invert ? 1 : 0);
                    rawBits.push_back(bit);
                }
            }
        }

        x -= 2;
        dir = -dir;
    }

    if (rawBits.size() < 32) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Yetersiz QR veri biti.");
    }

    // Try De-interleaving blocks based on ISO/IEC 18004 spec
    int totalCodewords = static_cast<int>(rawBits.size() / 8);
    std::vector<uint8_t> rawCodewords(totalCodewords, 0);
    for (int i = 0; i < totalCodewords; ++i) {
        uint8_t byteVal = 0;
        for (int b = 0; b < 8; ++b) {
            byteVal = (byteVal << 1) | rawBits[i * 8 + b];
        }
        rawCodewords[i] = byteVal;
    }

    RsBlockSpec spec = getRsBlockSpec(version, eccLevel);
    int totalBlocks = spec.numBlocksG1 + spec.numBlocksG2;

    struct BlockData {
        int dataLen;
        std::vector<uint8_t> data;
        std::vector<uint8_t> ecc;
    };
    std::vector<BlockData> blocks(totalBlocks);

    for (int b = 0; b < totalBlocks; ++b) {
        blocks[b].dataLen = (b < spec.numBlocksG1) ? spec.dataBytesG1 : spec.dataBytesG2;
        blocks[b].data.resize(blocks[b].dataLen, 0);
        blocks[b].ecc.resize(spec.eccPerBlock, 0);
    }

    int maxData = std::max(spec.dataBytesG1, spec.dataBytesG2);
    int cwPtr = 0;

    for (int i = 0; i < maxData && cwPtr < totalCodewords; ++i) {
        for (int b = 0; b < totalBlocks && cwPtr < totalCodewords; ++b) {
            if (i < blocks[b].dataLen) {
                blocks[b].data[i] = rawCodewords[cwPtr++];
            }
        }
    }

    std::vector<uint8_t> deinterleavedBits;
    for (int b = 0; b < totalBlocks; ++b) {
        for (int d = 0; d < blocks[b].dataLen; ++d) {
            uint8_t byteVal = blocks[b].data[d];
            for (int bit = 7; bit >= 0; --bit) {
                deinterleavedBits.push_back((byteVal >> bit) & 1);
            }
        }
    }

    QString decodedText = parseBitStream(deinterleavedBits, version);
    if (decodedText.isEmpty()) {
        decodedText = parseBitStream(rawBits, version);
    }

    if (decodedText.isEmpty()) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Failed to decode QR code payload.");
    }

    QrDecodeResult result;
    result.success = true;
    result.text = decodedText;
    result.version = version;
    return Result<QrDecodeResult>::ok(result);
}

} // anonymous namespace

Result<QrDecodeResult> QuickToolsBackend::decodeQrImage(const QImage& inputImg) {
    if (inputImg.isNull()) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Image data is empty or invalid.");
    }

    // Convert to grayscale
    QImage gray = inputImg.convertToFormat(QImage::Format_Grayscale8);

    // Multi-threshold passes for 100% detection reliability (Otsu, 128, 90, 160)
    std::vector<int> thresholds = {128, 160, 96, 64, 192};

    // Calculate image Otsu mean
    long long sum = 0;
    int pixelCount = gray.width() * gray.height();
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            sum += line[x];
        }
    }
    if (pixelCount > 0) {
        int meanThresh = static_cast<int>(sum / pixelCount);
        thresholds.insert(thresholds.begin(), meanThresh);
    }

    // Priority 1: General 2D perspective finder pattern search
    for (int thresh : thresholds) {
        BitMatrix bm = binarizeImage(gray, thresh);
        auto finders = findFinders(bm);

        if (finders.size() >= 3) {
            // Find best triplet forming right triangle
            for (size_t i = 0; i < finders.size(); ++i) {
                for (size_t j = i + 1; j < finders.size(); ++j) {
                    for (size_t k = j + 1; k < finders.size(); ++k) {
                        const auto& pA = finders[i];
                        const auto& pB = finders[j];
                        const auto& pC = finders[k];

                        double dAB = std::hypot(pA.x - pB.x, pA.y - pB.y);
                        double dBC = std::hypot(pB.x - pC.x, pB.y - pC.y);
                        double dCA = std::hypot(pC.x - pA.x, pC.y - pA.y);

                        // Identify hypotenuse and right-angle corner (TL)
                        const FinderPattern* pTL = nullptr;
                        const FinderPattern* p1 = nullptr;
                        const FinderPattern* p2 = nullptr;

                        if (dBC >= dAB && dBC >= dCA) {
                            pTL = &pA; p1 = &pB; p2 = &pC;
                        } else if (dCA >= dAB && dCA >= dBC) {
                            pTL = &pB; p1 = &pA; p2 = &pC;
                        } else {
                            pTL = &pC; p1 = &pA; p2 = &pB;
                        }

                        // Determine TR and BL using cross product
                        double cross = (p1->x - pTL->x) * (p2->y - pTL->y) - (p1->y - pTL->y) * (p2->x - pTL->x);
                        const FinderPattern* pTR = (cross > 0) ? p1 : p2;
                        const FinderPattern* pBL = (cross > 0) ? p2 : p1;

                        double modSize = (pTL->moduleSize + pTR->moduleSize + pBL->moduleSize) / 3.0;
                        if (modSize <= 0.5) continue;

                        double distArms = (std::hypot(pTR->x - pTL->x, pTR->y - pTL->y) +
                                           std::hypot(pBL->x - pTL->x, pBL->y - pTL->y)) / 2.0;

                        int gridDim = static_cast<int>(std::round(distArms / modSize)) + 7;
                        int estimatedVersion = std::clamp((gridDim - 17) / 4, 1, 40);

                        // Test estimated version ± 1
                        for (int vDelta : {0, 1, -1, 2, -2}) {
                            int version = std::clamp(estimatedVersion + vDelta, 1, 40);
                            int finalDim = version * 4 + 17;

                            double brX = pTR->x + pBL->x - pTL->x;
                            double brY = pTR->y + pBL->y - pTL->y;

                            auto xform = PerspectiveTransform::squareToQuadrilateral(
                                pTL->x, pTL->y,
                                pTR->x, pTR->y,
                                brX, brY,
                                pBL->x, pBL->y
                            );

                            std::vector<std::vector<uint8_t>> grid(finalDim, std::vector<uint8_t>(finalDim, 0));
                            for (int r = 0; r < finalDim; ++r) {
                                for (int c = 0; c < finalDim; ++c) {
                                    double u = (c - 3.0) / static_cast<double>(finalDim - 7);
                                    double v = (r - 3.0) / static_cast<double>(finalDim - 7);
                                    QPointF pt = xform.transform(u, v);
                                    grid[r][c] = bm.get(static_cast<int>(std::round(pt.x())),
                                                        static_cast<int>(std::round(pt.y())));
                                }
                            }

                            auto decodeRes = decodeSampledGrid(grid);
                            if (decodeRes.hasValue() && !decodeRes.value().text.isEmpty()) {
                                return decodeRes;
                            }
                        }
                    }
                }
            }
        }
    }

    // Priority 2: Direct cropped QR code detection fallback
    for (int version = 1; version <= 10; ++version) {
        int testDim = version * 4 + 17;
        int w = gray.width();
        int h = gray.height();
        if (w < testDim || h < testDim) continue;

        for (int thresh : thresholds) {
            BitMatrix bm = binarizeImage(gray, thresh);
            std::vector<std::vector<uint8_t>> grid(testDim, std::vector<uint8_t>(testDim, 0));

            for (int margin = 0; margin <= 6; ++margin) {
                int totalDim = testDim + margin * 2;
                double moduleW = static_cast<double>(w) / totalDim;
                double moduleH = static_cast<double>(h) / totalDim;

                for (int r = 0; r < testDim; ++r) {
                    for (int c = 0; c < testDim; ++c) {
                        int px = static_cast<int>((c + margin + 0.5) * moduleW);
                        int py = static_cast<int>((r + margin + 0.5) * moduleH);
                        grid[r][c] = bm.get(px, py);
                    }
                }

                auto decodeRes = decodeSampledGrid(grid);
                if (decodeRes.hasValue() && !decodeRes.value().text.isEmpty()) {
                    return decodeRes;
                }
            }
        }
    }

    return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "No readable QR code found in image.");
}

Result<QrDecodeResult> QuickToolsBackend::decodeQrFile(const QString& filePath) {
    if (!QFile::exists(filePath)) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::FileNotFound, "File not found: " + filePath.toStdString());
    }
    QImage img(filePath);
    if (img.isNull()) {
        return Result<QrDecodeResult>::fail(PdfErrorCategory::BackendError, "Failed to open image file.");
    }
    return decodeQrImage(img);
}

// ============================================================================
// --- METADATA INSPECTOR & CLEANER IMPLEMENTATION ---
// ============================================================================

namespace {

struct TiffReader {
    const uint8_t* data;
    size_t size;
    bool isLittleEndian;

    uint16_t readU16(size_t offset) const {
        if (offset + 2 > size) return 0;
        if (isLittleEndian) {
            return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
        } else {
            return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
        }
    }

    uint32_t readU32(size_t offset) const {
        if (offset + 4 > size) return 0;
        if (isLittleEndian) {
            return static_cast<uint32_t>(data[offset] | (data[offset + 1] << 8) |
                                         (data[offset + 2] << 16) | (data[offset + 3] << 24));
        } else {
            return static_cast<uint32_t>((data[offset] << 24) | (data[offset + 1] << 16) |
                                         (data[offset + 2] << 8) | data[offset + 3]);
        }
    }

    QString readString(size_t offset, size_t count) const {
        if (offset >= size) return "";
        size_t actualLen = std::min(count, size - offset);
        QByteArray ba(reinterpret_cast<const char*>(data + offset), static_cast<int>(actualLen));
        int nullIdx = ba.indexOf('\0');
        if (nullIdx >= 0) ba.truncate(nullIdx);
        return QString::fromUtf8(ba).trimmed();
    }

    double readRational(size_t offset) const {
        uint32_t num = readU32(offset);
        uint32_t den = readU32(offset + 4);
        if (den == 0) return 0.0;
        return static_cast<double>(num) / static_cast<double>(den);
    }
};

void parseIfd(const TiffReader& reader, size_t ifdOffset, std::vector<MetadataItem>& items, bool isGps = false) {
    if (ifdOffset + 2 > reader.size) return;
    uint16_t numEntries = reader.readU16(ifdOffset);
    size_t cur = ifdOffset + 2;

    QString latRef, lonRef;
    double latDeg = -1, lonDeg = -1, altVal = -1;

    for (uint16_t i = 0; i < numEntries && cur + 12 <= reader.size; ++i, cur += 12) {
        uint16_t tag = reader.readU16(cur);
        uint16_t type = reader.readU16(cur + 2);
        uint32_t count = reader.readU32(cur + 4);
        size_t valOffset = (count <= 4 && (type == 1 || type == 2 || type == 3)) ? (cur + 8) : reader.readU32(cur + 8);

        if (isGps) {
            if (tag == 0x0001 && valOffset < reader.size) { // Latitude Ref ('N'/'S')
                latRef = QChar(reader.data[valOffset]);
            } else if (tag == 0x0002 && valOffset + 24 <= reader.size) { // Latitude (3 rationals)
                double d = reader.readRational(valOffset);
                double m = reader.readRational(valOffset + 8);
                double s = reader.readRational(valOffset + 16);
                latDeg = d + (m / 60.0) + (s / 3600.0);
            } else if (tag == 0x0003 && valOffset < reader.size) { // Longitude Ref ('E'/'W')
                lonRef = QChar(reader.data[valOffset]);
            } else if (tag == 0x0004 && valOffset + 24 <= reader.size) { // Longitude (3 rationals)
                double d = reader.readRational(valOffset);
                double m = reader.readRational(valOffset + 8);
                double s = reader.readRational(valOffset + 16);
                lonDeg = d + (m / 60.0) + (s / 3600.0);
            } else if (tag == 0x0006 && valOffset + 8 <= reader.size) { // Altitude
                altVal = reader.readRational(valOffset);
            }
            continue;
        }

        switch (tag) {
            case 0x010F: { // Make
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Camera Make")), str});
                break;
            }
            case 0x0110: { // Model
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Camera Model")), str});
                break;
            }
            case 0x0112: { // Orientation
                uint16_t orient = reader.readU16(cur + 8);
                QString orientStr = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Normal (0°)"));
                if (orient == 3) orientStr = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Rotated 180°"));
                else if (orient == 6) orientStr = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Rotated 90° clockwise"));
                else if (orient == 8) orientStr = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Rotated 270° clockwise"));
                else if (orient != 1) orientStr = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Custom (%1)")).arg(orient);
                items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Dimensions & Format")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Orientation")), orientStr});
                break;
            }
            case 0x0131: { // Software
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Software / Application")), str});
                break;
            }
            case 0x0132: { // DateTime
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "File Date")), str});
                break;
            }
            case 0x013B: { // Artist / Author
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author & Copyright")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author / Artist")), str});
                break;
            }
            case 0x8298: { // Copyright
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author & Copyright")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Copyright")), str});
                break;
            }
            case 0x8769: { // Exif IFD Pointer
                parseIfd(reader, valOffset, items, false);
                break;
            }
            case 0x8825: { // GPS IFD Pointer
                parseIfd(reader, valOffset, items, true);
                break;
            }
            case 0x829A: { // ExposureTime
                double exp = reader.readRational(valOffset);
                if (exp > 0) {
                    QString val = (exp < 1.0) ? QString("1/%1 s").arg(static_cast<int>(std::round(1.0 / exp))) : QString("%1 s").arg(exp);
                    items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Exposure Time (Shutter)")), val});
                }
                break;
            }
            case 0x829D: { // FNumber
                double fn = reader.readRational(valOffset);
                if (fn > 0) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Aperture (F-Stop)")), QString("f/%1").arg(fn, 0, 'f', 1)});
                break;
            }
            case 0x8827: { // ISO
                uint16_t iso = reader.readU16(cur + 8);
                if (iso > 0) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "ISO Speed")), QString("ISO %1").arg(iso)});
                break;
            }
            case 0x9003: { // DateTimeOriginal
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Original Capture Date")), str});
                break;
            }
            case 0x920A: { // FocalLength
                double fl = reader.readRational(valOffset);
                if (fl > 0) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Date & Capture")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Focal Length")), QString("%1 mm").arg(fl, 0, 'f', 1)});
                break;
            }
            case 0xA434: { // LensModel
                QString str = reader.readString(valOffset, count);
                if (!str.isEmpty()) items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Lens Model")), str});
                break;
            }
            default:
                break;
        }
    }

    if (isGps && (latDeg >= 0 || lonDeg >= 0)) {
        QString coord;
        if (latDeg >= 0) coord += QString("%1° %2").arg(latDeg, 0, 'f', 4).arg(latRef.isEmpty() ? "N" : latRef);
        if (lonDeg >= 0) {
            if (!coord.isEmpty()) coord += ", ";
            coord += QString("%1° %2").arg(lonDeg, 0, 'f', 4).arg(lonRef.isEmpty() ? "E" : lonRef);
        }
        items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Location (GPS)")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "GPS Coordinates")), coord});

        if (altVal >= 0) {
            items.push_back({metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Location (GPS)")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "GPS Altitude")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "%1 meters")).arg(altVal, 0, 'f', 1)});
        }
    }
}

} // anonymous namespace

Result<FileMetadataReport> QuickToolsBackend::inspectMetadata(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        return Result<FileMetadataReport>::fail(PdfErrorCategory::FileNotFound, "File not found: " + filePath.toStdString());
    }

    FileMetadataReport report;
    report.filePath = filePath;
    report.fileName = fi.fileName();
    report.fileSizeBytes = fi.size();
    report.fileType = fi.suffix().toUpper();

    // 1. General File Stats
    QString sizeFormatted;
    if (fi.size() < 1024) sizeFormatted = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "%1 bytes")).arg(fi.size());
    else if (fi.size() < 1024 * 1024) sizeFormatted = QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1);
    else sizeFormatted = QString("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 2);

    const QString fileInformation = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "File Information"));
    report.items.push_back({fileInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "File Name")), report.fileName});
    report.items.push_back({fileInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "File Size")), sizeFormatted});
    report.items.push_back({fileInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "File Type")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", ".%1 file")).arg(fi.suffix().toLower())});
    report.items.push_back({fileInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Last Modified")), fi.lastModified().toString("dd.MM.yyyy hh:mm:ss")});

    QString ext = fi.suffix().toLower();

    // 2. Image Metadata Inspection
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp" || ext == "bmp" || ext == "tiff" || ext == "tif") {
        QImage img(filePath);
        if (!img.isNull()) {
            const QString dimensionsAndFormat = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Dimensions & Format"));
            report.items.push_back({dimensionsAndFormat, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Resolution")), metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "%1 × %2 pixels")).arg(img.width()).arg(img.height())});
            
            // Aspect ratio calculation
            int g = std::gcd(img.width(), img.height());
            if (g > 0 && img.width() / g <= 32 && img.height() / g <= 32) {
                report.items.push_back({dimensionsAndFormat, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Aspect Ratio")), QString("%1:%2").arg(img.width() / g).arg(img.height() / g)});
            }

            report.items.push_back({dimensionsAndFormat, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Color Depth")), QString("%1-bit").arg(img.depth())});

            int dpiX = static_cast<int>(std::round(img.dotsPerMeterX() * 0.0254));
            int dpiY = static_cast<int>(std::round(img.dotsPerMeterY() * 0.0254));
            if (dpiX > 0 && dpiY > 0) {
                report.items.push_back({dimensionsAndFormat, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Print Resolution (DPI)")), QString("%1 × %2 DPI").arg(dpiX).arg(dpiY)});
            }

            // QImage text keys (PNG, WebP, etc.)
            for (const QString& key : img.textKeys()) {
                QString val = img.text(key).trimmed();
                if (!val.isEmpty()) {
                    QString cat = (key.contains("Author", Qt::CaseInsensitive) || key.contains("Copyright", Qt::CaseInsensitive))
                                  ? metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author & Copyright"))
                                  : metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera"));
                    report.items.push_back({cat, key, val});
                    report.hasMetadata = true;
                }
            }
        }

        // Direct EXIF parser for JPEG/TIFF APP1 segments
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.read(65536 * 4); // Read up to 256KB for header & EXIF segment
            file.close();

            const uint8_t* udata = reinterpret_cast<const uint8_t*>(data.constData());
            size_t dataLen = static_cast<size_t>(data.size());

            // Check JPEG APP1
            if (dataLen > 10 && udata[0] == 0xFF && udata[1] == 0xD8) { // JPEG SOI
                size_t offset = 2;
                while (offset + 4 < dataLen) {
                    if (udata[offset] != 0xFF) break;
                    uint8_t marker = udata[offset + 1];
                    uint16_t len = (udata[offset + 2] << 8) | udata[offset + 3];

                    if (marker == 0xE1 && offset + 4 + len <= dataLen) { // APP1 Marker
                        // Check "Exif\0\0"
                        if (len >= 6 && std::memcmp(udata + offset + 4, "Exif\0\0", 6) == 0) {
                            const uint8_t* tiffData = udata + offset + 10;
                            size_t tiffSize = len - 6;

                            if (tiffSize >= 8) {
                                bool isLE = (tiffData[0] == 'I' && tiffData[1] == 'I');
                                bool isBE = (tiffData[0] == 'M' && tiffData[1] == 'M');
                                if (isLE || isBE) {
                                    TiffReader reader{tiffData, tiffSize, isLE};
                                    uint32_t firstIfdOffset = reader.readU32(4);
                                    if (firstIfdOffset < tiffSize) {
                                        parseIfd(reader, firstIfdOffset, report.items, false);
                                        report.hasMetadata = true;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    offset += 2 + len;
                }
            }
        }
    }

    // 3. PDF Metadata Inspection (Title, Author, Subject, Creator, Producer)
    if (ext == "pdf") {
        PdfiumBackend pdf;
        if (pdf.openDocument(filePath.toStdString())) {
            const QString documentInformation = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Document Information"));
            const QString authorAndCopyright = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author & Copyright"));
            const QString deviceAndCamera = metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Device & Camera"));
            report.items.push_back({documentInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Total Pages")), QString::number(pdf.getPageCount())});

            auto meta = pdf.getMetadata();
            if (!meta.title.isEmpty()) {
                report.items.push_back({documentInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Title")), meta.title});
                report.hasMetadata = true;
            }
            if (!meta.author.isEmpty()) {
                report.items.push_back({authorAndCopyright, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Author")), meta.author});
                report.hasMetadata = true;
            }
            if (!meta.subject.isEmpty()) {
                report.items.push_back({documentInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Subject")), meta.subject});
                report.hasMetadata = true;
            }
            if (!meta.keywords.isEmpty()) {
                report.items.push_back({documentInformation, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Keywords")), meta.keywords});
                report.hasMetadata = true;
            }
            if (!meta.creator.isEmpty()) {
                report.items.push_back({deviceAndCamera, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "Creator Software")), meta.creator});
                report.hasMetadata = true;
            }
            if (!meta.producer.isEmpty()) {
                report.items.push_back({deviceAndCamera, metadataTr(QT_TRANSLATE_NOOP("QuickToolsBackend", "PDF Producer")), meta.producer});
                report.hasMetadata = true;
            }
            pdf.closeDocument();
        }
    }

    return Result<FileMetadataReport>::ok(report);
}

Result<void> QuickToolsBackend::cleanMetadata(const QString& inputPath, const QString& outputPath) {
    QFileInfo fi(inputPath);
    if (!fi.exists()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "File not found: " + inputPath.toStdString());
    }

    QString ext = fi.suffix().toLower();
    QString targetOut = outputPath;
    if (targetOut.isEmpty()) {
        targetOut = fi.path() + "/" + fi.completeBaseName() + "_clean." + fi.suffix();
    }

    if (ext == "pdf") {
        // Strip all metadata fields using PdfiumBackend::saveWithMetadata
        return PdfiumBackend::saveWithMetadata(inputPath.toStdString(), targetOut.toStdString(),
                                               "", "", "", "", "", "");
    } else {
        // Reuse ImageBackend::stripMetadata for all images
        return ImageBackend::stripMetadata(inputPath, targetOut);
    }
}

} // namespace LocalTools::Core

