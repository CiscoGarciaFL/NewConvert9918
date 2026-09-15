#include "newconvert9918/core/Validation.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>
#include <string_view>

using newconvert9918::core::ConversionSettings;
using newconvert9918::core::validate;

namespace {

const QString corpusDirectory = QStringLiteral(NEWCONVERT9918_GOLDEN_DIR);

class TestContext final {
public:
    void expect(bool condition, std::string_view message)
    {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    [[nodiscard]] int result() const { return failures_ == 0 ? 0 : 1; }

private:
    int failures_ = 0;
};

QJsonObject loadCorpusManifest()
{
    QFile file(corpusDirectory + QStringLiteral("/corpus.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonObject loadCaptureManifest()
{
    QFile file(corpusDirectory + QStringLiteral("/reference/original-1_9_1/capture.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

QString corpusPath(const QJsonObject &entry)
{
    const QString repositoryRelativePath = entry.value(QStringLiteral("path")).toString();
    const QString prefix = QStringLiteral("tests/golden/");
    if (!repositoryRelativePath.startsWith(prefix)) {
        return {};
    }
    return corpusDirectory + QStringLiteral("/") + repositoryRelativePath.mid(prefix.size());
}

QByteArray sha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return hash.result().toHex();
}

void testSettingsValidation(TestContext &test)
{
    test.expect(validate(ConversionSettings{}).empty(), "default settings should be valid");

    ConversionSettings settings;
    settings.gamma = 0.0;
    auto issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "gamma",
                "zero gamma should produce one gamma issue");

    settings = ConversionSettings{};
    settings.maximumColorShiftPercent = 101.0;
    issues = validate(settings);
    test.expect(issues.size() == 1 && issues.front().field == "maximumColorShiftPercent",
                "color shift above 100 percent should produce one color-shift issue");
}

void testCorpusManifest(TestContext &test, const QJsonObject &manifest)
{
    test.expect(manifest.value(QStringLiteral("schema_version")).toInt() == 1,
                "corpus schema version should be 1");

    const QJsonArray sourceImages = manifest.value(QStringLiteral("source_images")).toArray();
    const QJsonArray malformedInputs = manifest.value(QStringLiteral("malformed_inputs")).toArray();
    test.expect(sourceImages.size() == 8, "corpus should contain eight source images");
    test.expect(malformedInputs.size() == 3, "corpus should contain three malformed inputs");

    for (const QJsonArray &entries : {sourceImages, malformedInputs}) {
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            const QString path = corpusPath(entry);
            test.expect(!path.isEmpty(), "corpus paths must remain under tests/golden");
            test.expect(sha256(path) == entry.value(QStringLiteral("sha256")).toString().toLatin1(),
                        "corpus SHA-256 digest should match the manifest");
        }
    }
}

void testCorpusImages(TestContext &test, const QJsonObject &manifest)
{
    const QJsonArray sourceImages = manifest.value(QStringLiteral("source_images")).toArray();
    for (const QJsonValue &value : sourceImages) {
        const QJsonObject entry = value.toObject();
        QImageReader reader(corpusPath(entry));
        const QImage image = reader.read();
        test.expect(!image.isNull(), "each corpus source should decode as an image");
        test.expect(image.size()
                        == QSize(entry.value(QStringLiteral("width")).toInt(),
                                 entry.value(QStringLiteral("height")).toInt()),
                    "decoded dimensions should match the manifest");
    }

    for (const QString &filename : {QStringLiteral("tiny-rgba.png"),
                                    QStringLiteral("transparency-rgba.png")}) {
        const QImage image(corpusDirectory + QStringLiteral("/source/") + filename);
        test.expect(!image.isNull() && image.hasAlphaChannel(),
                    "alpha fixtures should decode with an alpha channel");
    }

    const QImage gradient(corpusDirectory + QStringLiteral("/source/transparency-rgba.png"));
    test.expect(gradient.pixelColor(0, 0).alpha() == 0,
                "the transparency gradient should include fully transparent pixels");
    test.expect(gradient.pixelColor(gradient.width() / 2, gradient.height() / 2).alpha() >= 250,
                "the transparency gradient should include nearly opaque pixels");
}

void testMalformedInputs(TestContext &test, const QJsonObject &manifest)
{
    const QJsonArray malformedInputs = manifest.value(QStringLiteral("malformed_inputs")).toArray();
    for (const QJsonValue &value : malformedInputs) {
        QImageReader reader(corpusPath(value.toObject()));
        test.expect(reader.read().isNull(), "malformed corpus input should be rejected");
    }
}

void testOriginalCapture(TestContext &test)
{
    const QJsonObject capture = loadCaptureManifest();
    test.expect(capture.value(QStringLiteral("schema_version")).toInt() == 1,
                "original capture schema version should be 1");
    const QJsonObject original = capture.value(QStringLiteral("original")).toObject();
    test.expect(original.value(QStringLiteral("version")).toString() == QStringLiteral("1.9.1.0"),
                "original capture should identify Convert9918 1.9.1.0");

    const QJsonArray captures = capture.value(QStringLiteral("captures")).toArray();
    test.expect(captures.size() == 8, "original capture should contain all eight valid sources");
    for (const QJsonValue &captureValue : captures) {
        const QJsonObject captureEntry = captureValue.toObject();
        const QJsonArray outputs = captureEntry.value(QStringLiteral("outputs")).toArray();
        test.expect(outputs.size() == 3, "each original capture should contain three outputs");
        for (const QJsonValue &outputValue : outputs) {
            const QJsonObject output = outputValue.toObject();
            const QString path = corpusPath(output);
            const QFileInfo file(path);
            test.expect(file.exists(), "each recorded original output should exist");
            test.expect(file.size() == output.value(QStringLiteral("size")).toInteger(),
                        "original output size should match its capture manifest");
            test.expect(sha256(path) == output.value(QStringLiteral("sha256")).toString().toLatin1(),
                        "original output SHA-256 should match its capture manifest");

            const QString suffix = file.suffix().toUpper();
            if (suffix == QStringLiteral("BMP")) {
                const QImage preview(path);
                test.expect(preview.size() == QSize(256, 192),
                            "original BMP preview should be 256 by 192 pixels");
            } else {
                test.expect(suffix == QStringLiteral("TIAP") || suffix == QStringLiteral("TIAC"),
                            "original binary output should be a TIAP or TIAC table");
                test.expect(file.size() == 6272,
                            "TIFILES table should contain a 128-byte header and 6144-byte payload");
                QFile input(path);
                test.expect(input.open(QIODevice::ReadOnly), "TIFILES output should be readable");
                test.expect(input.read(8) == QByteArray("\x07TIFILES", 8),
                            "TIFILES output should contain the expected signature");
            }
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    const QCoreApplication application(argc, argv);
    TestContext test;
    testSettingsValidation(test);
    const QJsonObject manifest = loadCorpusManifest();
    testCorpusManifest(test, manifest);
    testCorpusImages(test, manifest);
    testMalformedInputs(test, manifest);
    testOriginalCapture(test);
    return test.result();
}
