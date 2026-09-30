#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>

#include <iostream>

namespace {

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

struct RunResult {
    int exitCode{-1};
    QByteArray standardOutput;
    QByteArray standardError;
    bool completed{};
};

RunResult runCli(const QStringList& arguments)
{
    QProcess process;
    process.setProgram(QStringLiteral(RETROVDP_CLI_PATH));
    process.setArguments(arguments);
    process.start();
    const bool started = process.waitForStarted(15'000);
    const bool finished = started && process.waitForFinished(90'000);
    return {
        .exitCode = finished ? process.exitCode() : -1,
        .standardOutput = process.readAllStandardOutput(),
        .standardError = process.readAllStandardError(),
        .completed = finished && process.exitStatus() == QProcess::NormalExit,
    };
}

QJsonObject jsonResult(const RunResult& run)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(run.standardOutput.trimmed(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    return document.object();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    TestContext test;

    const QString source = QDir(QStringLiteral(RETROVDP_GOLDEN_DIR))
                               .filePath(QStringLiteral("source/tiny-rgba.png"));
    QTemporaryDir output;
    test.expect(QFileInfo::exists(QStringLiteral(RETROVDP_CLI_PATH)),
                "CLI executable should exist");
    test.expect(QFileInfo::exists(source), "CLI source fixture should exist");
    test.expect(output.isValid(), "CLI output directory should be available");

    const RunResult help = runCli({QStringLiteral("--help")});
    if (!help.completed || help.exitCode != 0) {
        std::cerr << "CLI help launch failed; executable=" << RETROVDP_CLI_PATH
                  << " exit=" << help.exitCode
                  << " stderr=" << help.standardError.toStdString() << '\n';
    }
    test.expect(help.completed && help.exitCode == 0
                    && help.standardOutput.contains("--input")
                    && help.standardOutput.contains("--recipe")
                    && help.standardOutput.contains("--target")
                    && help.standardOutput.contains("--format"),
                "CLI help should describe its stable input and export options");

    const QStringList validArguments{
        QStringLiteral("--input"), source,
        QStringLiteral("--output"), output.path(),
        QStringLiteral("--target"), QStringLiteral("tms9918a"),
        QStringLiteral("--mode"), QStringLiteral("bitmap-9918a"),
        QStringLiteral("--preset"), QStringLiteral("crisp-pixel-art"),
        QStringLiteral("--format"), QStringLiteral("raw"),
        QStringLiteral("--json"),
    };
    const RunResult first = runCli(validArguments);
    const QJsonObject firstJson = jsonResult(first);
    const QJsonArray files = firstJson.value(QStringLiteral("files")).toArray();
    bool filesExist = files.size() == 2;
    for (const auto& entry : files) {
        const QJsonObject file = entry.toObject();
        filesExist &= QFileInfo::exists(file.value(QStringLiteral("path")).toString())
            && file.value(QStringLiteral("bytes")).toInteger() > 0;
    }
    test.expect(first.completed && first.exitCode == 0
                    && firstJson.value(QStringLiteral("status")) == QStringLiteral("ok")
                    && firstJson.value(QStringLiteral("mode"))
                        == QStringLiteral("bitmap-9918a")
                    && firstJson.value(QStringLiteral("target"))
                        == QStringLiteral("tms9918a")
                    && firstJson.value(QStringLiteral("preset"))
                        == QStringLiteral("crisp-pixel-art")
                    && filesExist,
                "CLI should load, convert, export, and report generated files as JSON");

    const RunResult conflict = runCli(validArguments);
    const QJsonObject conflictJson = jsonResult(conflict);
    test.expect(conflict.completed && conflict.exitCode == 6
                    && conflictJson.value(QStringLiteral("status"))
                        == QStringLiteral("error")
                    && conflictJson.value(QStringLiteral("code"))
                        == QStringLiteral("output-conflict")
                    && !conflictJson.value(QStringLiteral("details"))
                            .toObject().value(QStringLiteral("conflicts")).toArray().isEmpty(),
                "CLI should reject overwrites with a machine-readable conflict");

    QStringList overwriteArguments = validArguments;
    overwriteArguments.push_back(QStringLiteral("--overwrite"));
    const RunResult overwrite = runCli(overwriteArguments);
    test.expect(overwrite.completed && overwrite.exitCode == 0
                    && jsonResult(overwrite).value(QStringLiteral("status"))
                        == QStringLiteral("ok"),
                "CLI should replace files only when --overwrite is explicit");

    const QString recipePath = output.filePath(QStringLiteral("batch.rvdp.json"));
    QFile recipeFile(recipePath);
    const QJsonObject recipe{
        {QStringLiteral("kind"), QStringLiteral("newconvert9918-recipe")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("workspace"), QStringLiteral("screen-image")},
        {QStringLiteral("source"), QJsonObject{{QStringLiteral("path"), source}}},
        {QStringLiteral("conversion"),
         QJsonObject{{QStringLiteral("mode"), 0},
                     {QStringLiteral("targetProfile"), QStringLiteral("tms9918a")},
                     {QStringLiteral("dither"), 0},
                     {QStringLiteral("scalingFilter"), 0},
                     {QStringLiteral("fillMode"), 0},
                     {QStringLiteral("backgroundColor"), QStringLiteral("#000000")},
                     {QStringLiteral("exportFormat"), 2}}},
    };
    const bool recipeWritten = recipeFile.open(QIODevice::WriteOnly)
        && recipeFile.write(QJsonDocument(recipe).toJson()) > 0;
    recipeFile.close();
    const QString recipeOutput = output.filePath(QStringLiteral("recipe-output"));
    const RunResult recipeRun = runCli({
        QStringLiteral("--recipe"), recipePath,
        QStringLiteral("--output"), recipeOutput,
        QStringLiteral("--json"),
    });
    const QJsonObject recipeJson = jsonResult(recipeRun);
    test.expect(recipeWritten && recipeRun.completed && recipeRun.exitCode == 0
                    && recipeJson.value(QStringLiteral("preset"))
                        == QStringLiteral("recipe")
                    && recipeJson.value(QStringLiteral("mode"))
                        == QStringLiteral("bitmap-9918a")
                    && recipeJson.value(QStringLiteral("target"))
                        == QStringLiteral("tms9918a")
                    && recipeJson.value(QStringLiteral("format"))
                        == QStringLiteral("raw"),
                "CLI should migrate a versioned recipe saved under the retired identity");

    const RunResult badMode = runCli({
        QStringLiteral("--input"), source,
        QStringLiteral("--output"), output.path(),
        QStringLiteral("--mode"), QStringLiteral("not-a-mode"),
        QStringLiteral("--json"),
    });
    test.expect(badMode.completed && badMode.exitCode == 2
                    && jsonResult(badMode).value(QStringLiteral("code"))
                        == QStringLiteral("unknown-mode"),
                "CLI should use the documented usage exit code for an unknown mode");

    const RunResult missingInput = runCli({
        QStringLiteral("--input"), output.filePath(QStringLiteral("missing.png")),
        QStringLiteral("--output"), output.path(),
        QStringLiteral("--json"),
    });
    test.expect(missingInput.completed && missingInput.exitCode == 3
                    && jsonResult(missingInput).value(QStringLiteral("code"))
                        == QStringLiteral("input-failed"),
                "CLI should use the documented input exit code when loading fails");

    if (test.failures == 0) {
        std::cout << "CLI workflow validation passed\n";
    }
    return test.failures == 0 ? 0 : 1;
}
