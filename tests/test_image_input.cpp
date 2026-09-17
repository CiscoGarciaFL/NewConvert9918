#include "newconvert9918/core/ImageDecoding.hpp"
#include "newconvert9918/imageio/ImageLoader.hpp"

#include <QBuffer>
#include <QColorSpace>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
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

std::span<const std::uint8_t> bytes(const std::vector<std::uint8_t>& value)
{
    return value;
}

std::array<std::uint8_t, 3> pixel(const core::RgbImage& image,
                                  std::uint32_t x,
                                  std::uint32_t y)
{
    const auto row = image.row(y);
    const std::size_t offset = static_cast<std::size_t>(x)
        * core::bytesPerPixel(image.pixelFormat());
    return {row[offset], row[offset + 1], row[offset + 2]};
}

void writeLittle16(std::vector<std::uint8_t>& target,
                   std::size_t offset,
                   std::uint16_t value)
{
    target[offset] = static_cast<std::uint8_t>(value & 0xffU);
    target[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

std::vector<std::uint8_t> indexedPcx(bool compressed)
{
    std::vector<std::uint8_t> pcx(128, 0);
    pcx[0] = 0x0a;
    pcx[1] = 5;
    pcx[2] = compressed ? 1 : 0;
    pcx[3] = 8;
    writeLittle16(pcx, 8, 1); // xMax: 2 pixels
    pcx[65] = 1;
    writeLittle16(pcx, 66, 2);
    if (compressed) {
        pcx.insert(pcx.end(), {0xc1, 1, 0xc1, 2});
    } else {
        pcx.insert(pcx.end(), {1, 2});
    }
    pcx.push_back(0x0c);
    const std::size_t palette = pcx.size();
    pcx.resize(pcx.size() + 768, 0);
    pcx[palette + 3] = 10;
    pcx[palette + 4] = 20;
    pcx[palette + 5] = 30;
    pcx[palette + 6] = 40;
    pcx[palette + 7] = 50;
    pcx[palette + 8] = 60;
    return pcx;
}

void testPcx(TestContext& test)
{
    for (const bool compressed : {false, true}) {
        const auto pcx = indexedPcx(compressed);
        const auto result = core::decodePcx(bytes(pcx));
        test.expect(result && result.image->width() == 2 && result.image->height() == 1,
                    "indexed PCX should decode its declared dimensions");
        test.expect(result && pixel(*result.image, 0, 0) == std::array<std::uint8_t, 3>{10, 20, 30}
                        && pixel(*result.image, 1, 0)
                            == std::array<std::uint8_t, 3>{40, 50, 60},
                    "indexed PCX should apply its trailing palette");
    }

    auto truncated = indexedPcx(true);
    truncated.resize(130);
    test.expect(!core::decodePcx(bytes(truncated)),
                "PCX with a missing palette should be rejected");
    auto malformed = indexedPcx(false);
    malformed[66] = 1;
    malformed[67] = 0;
    test.expect(!core::decodePcx(bytes(malformed)),
                "PCX with a short scanline stride should be rejected");
    core::ImageSizeLimits tight;
    tight.maximumWidth = 1;
    test.expect(core::decodePcx(bytes(indexedPcx(false)), tight).error
                    == core::ImageDecodeError::AllocationLimitExceeded,
                "PCX should honor configured decoded-image limits");
}

std::vector<std::uint8_t> patternTable()
{
    std::vector<std::uint8_t> pattern(6144, 0);
    pattern[0] = 0x80;
    return pattern;
}

std::vector<std::uint8_t> colorTable()
{
    return std::vector<std::uint8_t>(6144, 0xf1);
}

void testGraphics2(TestContext& test)
{
    const auto pattern = patternTable();
    const auto colors = colorTable();
    const auto result = core::decodeGraphics2(bytes(pattern), bytes(colors));
    test.expect(result && result.image->width() == 256 && result.image->height() == 192,
                "Graphics II tables should decode to 256x192");
    test.expect(result && pixel(*result.image, 0, 0)
                    == std::array<std::uint8_t, 3>{248, 248, 248}
                    && pixel(*result.image, 1, 0)
                    == std::array<std::uint8_t, 3>{0, 0, 0},
                "Graphics II should apply foreground and background hardware colors");

    std::vector<std::uint8_t> palette(32, 0);
    palette[30] = 0x0f;
    palette[31] = 0x00;
    const auto f18a = core::decodeGraphics2(bytes(pattern), bytes(colors), bytes(palette));
    test.expect(f18a && pixel(*f18a.image, 0, 0)
                    == std::array<std::uint8_t, 3>{255, 0, 0},
                "Graphics II should decode a 32-byte F18A RGB444 palette");

    std::vector<std::uint8_t> half(2048, 0x22);
    const auto halfResult = core::decodeGraphics2(
        bytes(pattern), bytes(colors), {}, bytes(half));
    test.expect(halfResult && pixel(*halfResult.image, 1, 0)
                    == std::array<std::uint8_t, 3>{16, 100, 32},
                "Half Multicolor should average overlay and underlay colors");

    test.expect(!core::decodeGraphics2(
                    std::span(pattern).first(6143), bytes(colors)),
                "truncated Graphics II tables should be rejected");
    test.expect(!core::decodeGraphics2(
                    bytes(pattern), bytes(colors), std::span(palette).first(31)),
                "unsupported F18A palette sizes should be rejected");
}

void testStandaloneRetroLayouts(TestContext& test)
{
    const auto patterns = patternTable();
    const auto colors = colorTable();

    std::vector<std::uint8_t> sc2(0x2007 + 6144, 0);
    std::copy(patterns.begin(), patterns.end(), sc2.begin() + 7);
    std::copy(colors.begin(), colors.end(), sc2.begin() + 0x2007);
    test.expect(static_cast<bool>(core::decodeMsxScreen2(bytes(sc2))),
                "MSX Screen 2 layout should decode");
    test.expect(!core::decodeMsxScreen2(std::span(sc2).first(sc2.size() - 1)),
                "truncated MSX Screen 2 should be rejected");

    std::vector<std::uint8_t> pc;
    pc.insert(pc.end(), patterns.begin(), patterns.end());
    pc.insert(pc.end(), colors.begin(), colors.end());
    test.expect(static_cast<bool>(core::decodeColecoCvPaint(bytes(pc))),
                "Coleco CVPaint layout should decode");
    test.expect(!core::decodeColecoCvPaint(std::span(pc).first(pc.size() - 1)),
                "truncated Coleco CVPaint should be rejected");

    std::vector<std::uint8_t> pp(0x2800, 0);
    std::copy_n(patterns.begin(), 0x1400, pp.begin());
    std::copy_n(colors.begin(), 0x1400, pp.begin() + 0x1400);
    test.expect(static_cast<bool>(core::decodeAdamPowerPaint(bytes(pp))),
                "Adam PowerPaint layout should decode");
    test.expect(!core::decodeAdamPowerPaint(std::span(pp).first(pp.size() - 1)),
                "truncated Adam PowerPaint should be rejected");

    std::vector<std::uint8_t> hgr(0x15 + 0x2800, 0);
    std::copy_n(colors.begin(), 0x1400, hgr.begin() + 0x15);
    std::copy_n(patterns.begin(), 0x1400, hgr.begin() + 0x1415);
    test.expect(static_cast<bool>(core::decodeAdamHgr(bytes(hgr))),
                "Adam HGR/HGRH layout should decode");
    test.expect(!core::decodeAdamHgr(std::span(hgr).first(hgr.size() - 1)),
                "truncated Adam HGR should be rejected");
}

QByteArray encodeImage(const QImage& image, const char* format)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    return image.save(&buffer, format) ? bytes : QByteArray{};
}

void testQtRasterAdapter(TestContext& test)
{
    const QList<QByteArray> readable = QImageReader::supportedImageFormats();
    for (const QByteArray required : {QByteArray("png"), QByteArray("jpeg"),
                                      QByteArray("bmp"), QByteArray("gif")}) {
        test.expect(readable.contains(required),
                    "Qt installation should provide each required common raster reader");
    }
    QImage source(2, 1, QImage::Format_RGBA8888);
    source.setColorSpace(QColorSpace::SRgb);
    source.setPixelColor(0, 0, QColor(10, 20, 30, 40));
    source.setPixelColor(1, 0, QColor(50, 60, 70, 255));
    const QByteArray png = encodeImage(source, "PNG");
    const auto result = imageio::loadRasterData(png, "png");
    test.expect(result && result.metadata.format == imageio::ImageSourceFormat::Png
                    && result.metadata.sourceHadAlpha,
                "Qt PNG adapter should preserve alpha metadata");
    test.expect(result && result.image->pixelFormat() == core::PixelFormat::Rgba8888
                    && result.image->row(0)[3] == 40,
                "Qt PNG adapter should preserve RGBA pixels");

    for (const char* format : {"BMP", "JPG"}) {
        const QByteArray encoded = encodeImage(source, format);
        test.expect(!encoded.isEmpty(), "Qt test image format should encode");
        const auto decoded = imageio::loadRasterData(encoded, QByteArray(format).toLower());
        test.expect(static_cast<bool>(decoded), "Qt common raster format should decode");
    }

    const QList<QByteArray> writable = QImageWriter::supportedImageFormats();
    for (const QByteArray optional : {QByteArray("tiff"), QByteArray("webp")}) {
        if (readable.contains(optional) && writable.contains(optional)) {
            const QByteArray encoded = encodeImage(source, optional.constData());
            test.expect(!encoded.isEmpty()
                            && static_cast<bool>(imageio::loadRasterData(encoded, optional)),
                        "available Qt Image Formats plug-ins should round-trip");
        }
    }

    const QByteArray onePixelGif = QByteArray::fromHex(
        "47494638396101000100800000000000ffffff21f90401000000002c00000000010001000002024401003b");
    const auto gif = imageio::loadRasterData(onePixelGif, "gif");
    test.expect(gif && gif.metadata.format == imageio::ImageSourceFormat::Gif,
                "Qt GIF reader should decode the first frame");

    imageio::ImageLoadLimits inputLimit;
    inputLimit.maximumInputBytes = 8;
    test.expect(!imageio::loadRasterData(png, "png", inputLimit),
                "Qt raster adapter should enforce input byte limits before decoding");
    imageio::ImageLoadLimits dimensionLimit;
    dimensionLimit.decodedImage.maximumWidth = 1;
    test.expect(!imageio::loadRasterData(png, "png", dimensionLimit),
                "Qt raster adapter should enforce declared dimension limits");
    test.expect(!imageio::loadRasterData(QByteArray("not an image"), "png"),
                "malformed Qt raster input should be rejected");

    const auto clipboard = imageio::loadClipboardImage(source);
    test.expect(clipboard && clipboard.metadata.format
                    == imageio::ImageSourceFormat::Clipboard,
                "clipboard images should use the same bounded conversion path");
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "Could not open test file: " << file.errorString().toStdString() << '\n';
        return false;
    }
    const qint64 written = file.write(bytes);
    if (written != bytes.size()) {
        std::cerr << "Could not write test file: " << file.errorString().toStdString() << '\n';
        return false;
    }
    return file.flush();
}

QByteArray byteArray(const std::vector<std::uint8_t>& value)
{
    return QByteArray(reinterpret_cast<const char*>(value.data()),
                      static_cast<qsizetype>(value.size()));
}

QByteArray tiRle(std::uint8_t value)
{
    QByteArray encoded;
    std::size_t remaining = 6144;
    while (remaining != 0) {
        const std::size_t count = std::min<std::size_t>(remaining, 127);
        encoded.append(static_cast<char>(0x80U | count));
        encoded.append(static_cast<char>(value));
        remaining -= count;
    }
    return encoded;
}

void testFileRouting(TestContext& test)
{
    QTemporaryDir directory(QDir::current().filePath(
        QStringLiteral("image-input-test-XXXXXX")));
    test.expect(directory.isValid(), "temporary image-input directory should be available");
    const auto patterns = patternTable();
    const auto colors = colorTable();

    QByteArray header(128, 0);
    header[0] = 7;
    std::copy_n("TIFILES", 7, header.begin() + 1);
    const QString patternPath = directory.filePath(QStringLiteral("TEST.TIAP"));
    const QString colorPath = directory.filePath(QStringLiteral("TEST.TIAC"));
    test.expect(writeFile(patternPath, header + byteArray(patterns))
                    && writeFile(colorPath, header + byteArray(colors)),
                "TI Artist companions should be written for the routing test");
    const auto ti = imageio::loadImageFile(colorPath);
    test.expect(ti && ti.metadata.format == imageio::ImageSourceFormat::TiArtist,
                "selecting any TI Artist companion should load the complete set");

    const QString rlePatternPath = directory.filePath(QStringLiteral("RLE_P"));
    test.expect(writeFile(rlePatternPath, tiRle(0)),
                "TI Artist RLE fixture should be written");
    test.expect(static_cast<bool>(imageio::loadImageFile(rlePatternPath)),
                "TI Artist RLE pattern data should decode safely");

    const QString pcxPath = directory.filePath(QStringLiteral("test.pcx"));
    test.expect(writeFile(pcxPath, byteArray(indexedPcx(true))),
                "PCX routing fixture should be written");
    test.expect(imageio::loadImageFile(pcxPath).metadata.format
                    == imageio::ImageSourceFormat::Pcx,
                "PCX files should route to the independent decoder");

    test.expect(writeFile(colorPath, QByteArray(20, 0)),
                "truncated TI Artist companion should be written");
    test.expect(!imageio::loadImageFile(patternPath),
                "a truncated TI Artist companion should reject the set");
    test.expect(!imageio::loadImageFile(directory.filePath(QStringLiteral("missing.png"))),
                "missing image files should be rejected");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    TestContext test;
    testPcx(test);
    testGraphics2(test);
    testStandaloneRetroLayouts(test);
    testQtRasterAdapter(test);
    testFileRouting(test);
    if (test.failures == 0) {
        std::cout << "All image input tests passed.\n";
    }
    return test.failures == 0 ? 0 : 1;
}
