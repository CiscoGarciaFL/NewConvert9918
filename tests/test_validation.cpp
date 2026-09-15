#include "newconvert9918/core/Validation.hpp"

#include <QtTest/QTest>

using newconvert9918::core::ConversionSettings;
using newconvert9918::core::validate;

class ValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultSettingsAreValid()
    {
        const ConversionSettings settings;
        QVERIFY(validate(settings).empty());
    }

    void rejectsInvalidGamma()
    {
        ConversionSettings settings;
        settings.gamma = 0.0;

        const auto issues = validate(settings);
        QCOMPARE(issues.size(), std::size_t{1});
        QVERIFY(issues.front().field == "gamma");
    }

    void rejectsColorShiftOutsidePercentageRange()
    {
        ConversionSettings settings;
        settings.maximumColorShiftPercent = 101.0;

        const auto issues = validate(settings);
        QCOMPARE(issues.size(), std::size_t{1});
        QVERIFY(issues.front().field == "maximumColorShiftPercent");
    }
};

QTEST_MAIN(ValidationTests)
#include "test_validation.moc"
