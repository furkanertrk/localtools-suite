#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>
#include <iostream>

namespace {

QStringList placeholders(const QString& text) {
    QStringList values;
    QRegularExpression expression(QStringLiteral("%[1-9][0-9]*"));
    auto matches = expression.globalMatch(text);
    while (matches.hasNext()) values.push_back(matches.next().captured());
    std::sort(values.begin(), values.end());
    return values;
}

bool fail(const QString& message) {
    std::cerr << "[FAIL] " << message.toStdString() << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) return fail(QStringLiteral("translation catalog path is required")) ? 0 : 1;

    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("translation catalog could not be opened")) ? 0 : 1;
    }

    QXmlStreamReader xml(&file);
    QString currentContext;
    QString source;
    QString translation;
    QString translationType;
    int messageCount = 0;
    int translatedCount = 0;

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;

        if (xml.name() == QStringLiteral("name")) {
            currentContext = xml.readElementText();
        } else if (xml.name() == QStringLiteral("message")) {
            source.clear();
            translation.clear();
            translationType.clear();
            while (!(xml.isEndElement() && xml.name() == QStringLiteral("message"))
                   && !xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) continue;
                if (xml.name() == QStringLiteral("source")) {
                    source = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                } else if (xml.name() == QStringLiteral("translation")) {
                    translationType = xml.attributes().value(QStringLiteral("type")).toString();
                    translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                }
            }

            ++messageCount;
            const QString location = currentContext + QStringLiteral(" :: ") + source;
            if (translationType == QStringLiteral("unfinished")) {
                return fail(QStringLiteral("unfinished translation: ") + location) ? 0 : 1;
            }
            if (translation.trimmed().isEmpty()) {
                return fail(QStringLiteral("empty translation: ") + location) ? 0 : 1;
            }
            const QStringList sourcePlaceholders = placeholders(source);
            const QStringList translatedPlaceholders = placeholders(translation);
            for (const QString& placeholder : sourcePlaceholders) {
                if (sourcePlaceholders.count(placeholder)
                    != translatedPlaceholders.count(placeholder)) {
                    return fail(QStringLiteral("placeholder mismatch: ") + location) ? 0 : 1;
                }
            }
            ++translatedCount;
        }
    }

    if (xml.hasError()) {
        return fail(QStringLiteral("invalid TS XML: ") + xml.errorString()) ? 0 : 1;
    }
    if (messageCount == 0 || translatedCount != messageCount) {
        return fail(QStringLiteral("translation catalog contains no verified messages")) ? 0 : 1;
    }

    std::cout << "[PASS] " << translatedCount
              << " translations are complete and preserve all placeholders.\n";
    return 0;
}
