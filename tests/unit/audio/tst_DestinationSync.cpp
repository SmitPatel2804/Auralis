#include <auralis/audio/DestinationSync.h>

#include <QtTest>

#include <cmath>

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

    void estimateLagFindsInsertedDelay()
    {
        constexpr int rate = 16000;
        QVector<float> reference(rate);
        QVector<float> observed(rate);
        for (int i = 0; i < rate; ++i) {
            const float sample = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / rate));
            reference[i] = sample;
            const int delayed = i - 640; // 40 ms at 16 kHz
            observed[i] = delayed >= 0 ? reference[delayed] : 0.0f;
        }
        const auto lag = auralis::audio::estimateLagMs(reference, observed, rate);
        QVERIFY(lag.has_value());
        QVERIFY(std::abs(*lag - 40.0) < 3.0);
    }

    void silentBuffersAreNotALagEstimate()
    {
        const QVector<float> silence(8000, 0.0f);
        QVERIFY(!auralis::audio::estimateLagMs(silence, silence, 8000).has_value());
    }

    void runtimeAverageUsesTenSecondWindow()
    {
        auralis::audio::RuntimeLagAverager averager;
        averager.add(QStringLiteral("fast"), 100.0, 0);
        averager.add(QStringLiteral("slow"), 140.0, 0);
        QVERIFY(averager.hasTwoEndpoints(0));
        QCOMPARE(averager.averagedDelayMs(0).value(QStringLiteral("fast")), 100.0);
        averager.add(QStringLiteral("fast"), 999.0, 11000);
        QCOMPARE(averager.averagedDelayMs(11000).value(QStringLiteral("fast")), 250.0);
        QVERIFY(!averager.hasTwoEndpoints(11000));
        averager.add(QStringLiteral("slow"), 180.0, 11000);
        const auto pads = destinationDelayMsTowardMax(averager.averagedLatencySamples(11000));
        QCOMPARE(pads.value(QStringLiteral("fast")), 0.0);
        QVERIFY(pads.value(QStringLiteral("slow")) > 0.0);
    }
};

QTEST_GUILESS_MAIN(TstDestinationSync)
#include "tst_DestinationSync.moc"
