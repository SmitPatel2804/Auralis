#include <auralis/audio/DestinationSync.h>

#include <QtTest>

using auralis::audio::DestinationLatencySample;
using auralis::audio::destinationDelayMsTowardMax;

class TstDestinationSync : public QObject {
    Q_OBJECT

private slots:
    void specExampleDelaysFasterPathsTowardMax()
    {
        const QVector<DestinationLatencySample> samples{
            {QStringLiteral("a"), 100'000'000},
            {QStringLiteral("b"), 135'000'000},
            {QStringLiteral("c"), 117'000'000},
        };
        const auto delays = destinationDelayMsTowardMax(samples);
        QCOMPARE(delays.value(QStringLiteral("a")), 35.0);
        QCOMPARE(delays.value(QStringLiteral("b")), 0.0);
        QCOMPARE(delays.value(QStringLiteral("c")), 18.0);
    }

    void equalHostLatenciesNeedNoPad()
    {
        const QVector<DestinationLatencySample> samples{
            {QStringLiteral("buds"), 189'000'000},
            {QStringLiteral("rockerz"), 189'000'000},
        };
        const auto delays = destinationDelayMsTowardMax(samples);
        QCOMPARE(delays.value(QStringLiteral("buds")), 0.0);
        QCOMPARE(delays.value(QStringLiteral("rockerz")), 0.0);
    }

    void singleObservationIsNotCompensation()
    {
        const auto delays = destinationDelayMsTowardMax({{QStringLiteral("only"), 80'000'000}});
        QCOMPARE(delays.value(QStringLiteral("only")), 0.0);
    }

    void unknownSamplesStayAtZero()
    {
        const QVector<DestinationLatencySample> samples{
            {QStringLiteral("known-fast"), 50'000'000},
            {QStringLiteral("known-slow"), 90'000'000},
            {QStringLiteral("unknown"), std::nullopt},
        };
        const auto delays = destinationDelayMsTowardMax(samples);
        QCOMPARE(delays.value(QStringLiteral("known-fast")), 40.0);
        QCOMPARE(delays.value(QStringLiteral("known-slow")), 0.0);
        QCOMPARE(delays.value(QStringLiteral("unknown")), 0.0);
    }

    void subMillisecondGapIsIgnored()
    {
        const QVector<DestinationLatencySample> samples{
            {QStringLiteral("a"), 189'000'000},
            {QStringLiteral("b"), 189'400'000},
        };
        const auto delays = destinationDelayMsTowardMax(samples);
        QCOMPARE(delays.value(QStringLiteral("a")), 0.0);
        QCOMPARE(delays.value(QStringLiteral("b")), 0.0);
    }
};

QTEST_GUILESS_MAIN(TstDestinationSync)
#include "tst_DestinationSync.moc"
