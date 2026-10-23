#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

#include "services/file_service.h"
#include "ui/common/output_file_dialog.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool touch(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write("test") == 4;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir outputRoot;
    if (!check(outputRoot.isValid(), "temporary output directory must be available")) return 1;

    auto& files = LocalTools::Services::FileService::instance();
    const QString desired = QDir(outputRoot.path()).absoluteFilePath(QStringLiteral("çıktı.pdf"));
    if (!check(files.generateUniqueOutputPath(desired) == desired,
               "unused output name must be preserved")) return 1;
    if (!check(touch(desired), "first collision fixture must be created")) return 1;
    if (!check(touch(QDir(outputRoot.path()).absoluteFilePath(QStringLiteral("çıktı_1.pdf"))),
               "second collision fixture must be created")) return 1;

    const QString expected = QDir(outputRoot.path()).absoluteFilePath(QStringLiteral("çıktı_2.pdf"));
    if (!check(files.generateUniqueOutputPath(desired) == expected,
               "collisions must receive the first available deterministic suffix")) return 1;

    const QString equivalent = QDir(outputRoot.path()).absoluteFilePath(QStringLiteral("sub/../çıktı.pdf"));
    if (!check(LocalTools::UI::OutputFileDialog::refersToProtectedInput(equivalent, {desired}),
               "normalized source path must be protected from replacement")) return 1;
    if (!check(!LocalTools::UI::OutputFileDialog::refersToProtectedInput(expected, {desired}),
               "a distinct suffixed output path must be allowed")) return 1;

    std::cout << "[PASS] Output naming is collision-free and source files are protected.\n";
    return 0;
}
