#include "newconvert9918/imageio/ExportWriter.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace newconvert9918;

struct TestContext {
    int failures{};

    void expect(bool condition, const char* message)
    {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }
};

core::RgbImage preview()
{
    std::vector<std::uint8_t> pixels{
        255, 0, 0, 255,
        0, 255, 0, 128,
    };
    return *core::RgbImage::create(
        {.width = 2,
         .height = 1,
         .pixelFormat = core::PixelFormat::Rgba8888,
         .rowStride = 8},
        std::move(pixels));
}

void testPng(TestContext& test)
{
    const auto image = preview();
    const auto manifest = imageio::generatePngExport({
        .format = formats::ExportFormat::Png,
        .baseName = "preview",
        .preview = &image,
    });
    test.expect(static_cast<bool>(manifest), "PNG generation should succeed");
    test.expect(manifest.files.size() == 1 && manifest.files.front().fileName == "preview.png",
                "PNG generation should produce the expected file name");
    const auto& encoded = manifest.files.front().bytes;
    test.expect(encoded.size() > 8 && encoded[0] == 0x89 && encoded[1] == 'P'
                    && encoded[2] == 'N' && encoded[3] == 'G',
                "PNG generation should emit a PNG signature");

    QImage decoded;
    decoded.loadFromData(reinterpret_cast<const uchar*>(encoded.data()),
                         static_cast<int>(encoded.size()), "PNG");
    decoded = decoded.convertToFormat(QImage::Format_RGBA8888);
    test.expect(decoded.width() == 2 && decoded.height() == 1,
                "generated PNG should preserve preview dimensions");
    test.expect(decoded.pixelColor(0, 0) == QColor(255, 0, 0, 255)
                    && decoded.pixelColor(1, 0) == QColor(0, 255, 0, 128),
                "generated PNG should preserve RGB and alpha values");
}

void testWritingAndOverwrite(TestContext& test)
{
    QTemporaryDir directory(QDir::current().filePath(QStringLiteral("export-writer-XXXXXX")));
    test.expect(directory.isValid(), "temporary export directory should be available");
    formats::GeneratedFileManifest manifest{
        .format = formats::ExportFormat::Raw,
        .files = {{"IMAGE.TIAP", {1, 2, 3}}, {"IMAGE.TIAC", {4, 5}}},
    };
    const auto first = imageio::writeExportManifest(directory.path(), manifest);
    if (!first) std::cerr << "writer error: " << first.error.toStdString() << '\n';
    test.expect(first.status == imageio::ExportWriteStatus::Written && first.paths.size() == 2,
                "manifest writer should write every generated file");

    QFile pattern(directory.filePath(QStringLiteral("IMAGE.TIAP")));
    test.expect(pattern.open(QIODevice::ReadOnly) && pattern.readAll() == QByteArray("\1\2\3", 3),
                "manifest writer should preserve file bytes");
    pattern.close();

    const auto conflict = imageio::writeExportManifest(directory.path(), manifest);
    if (conflict.status != imageio::ExportWriteStatus::WouldOverwrite) {
        std::cerr << "conflict error: " << conflict.error.toStdString() << '\n';
    }
    test.expect(conflict.status == imageio::ExportWriteStatus::WouldOverwrite
                    && conflict.conflicts.size() == 2,
                "manifest writer should preflight and report every overwrite");

    manifest.files.front().bytes = {9};
    const auto overwrite = imageio::writeExportManifest(directory.path(), manifest, true);
    if (!overwrite) std::cerr << "overwrite error: " << overwrite.error.toStdString() << '\n';
    test.expect(overwrite.status == imageio::ExportWriteStatus::Written,
                "explicit overwrite should replace existing files");
    test.expect(pattern.open(QIODevice::ReadOnly) && pattern.readAll() == QByteArray("\11", 1),
                "explicit overwrite should commit the replacement bytes");
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    TestContext test;
    testPng(test);
    testWritingAndOverwrite(test);
    if (test.failures != 0) {
        std::cerr << test.failures << " export-writer test(s) failed\n";
        return 1;
    }
    std::cout << "All export-writer tests passed\n";
    return 0;
}
