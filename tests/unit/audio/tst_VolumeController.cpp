#include "FakePipeWireLinkBackend.h"

#include <auralis/audio/VolumeController.h>

#include <QtTest>
#include <limits>

using auralis::audio::RouteError;
using auralis::audio::VolumeController;
using auralis::test::FakePipeWireLinkBackend;

class TstVolumeController : public QObject {
    Q_OBJECT

private slots:
    void clampFiniteRange()
    {
        QCOMPARE(VolumeController::clamp(-0.5), 0.0);
        QCOMPARE(VolumeController::clamp(0.0), 0.0);
        QCOMPARE(VolumeController::clamp(0.4), 0.4);
        QCOMPARE(VolumeController::clamp(1.0), 1.0);
        QCOMPARE(VolumeController::clamp(2.5), 1.0);
        QCOMPARE(VolumeController::clamp(std::numeric_limits<double>::quiet_NaN()), 0.0);
        QCOMPARE(VolumeController::clamp(std::numeric_limits<double>::infinity()), 0.0);
    }

    void destinationVolumeAndMute()
    {
        FakePipeWireLinkBackend backend(nullptr);
        VolumeController volume(&backend);
        const auto result = volume.setDestinationVolume(4, QStringLiteral("dest-a"), 1.5);
        QVERIFY(result.allSucceeded);
        QCOMPARE(backend.lastVolumeNode, static_cast<quint32>(4));
        QCOMPARE(backend.lastVolume, 1.0);
        const auto muted = volume.setDestinationMuted(4, QStringLiteral("dest-a"), true);
        QVERIFY(muted.allSucceeded);
        QVERIFY(backend.lastMuted);
    }

    void unsupportedDoesNotCrash()
    {
        FakePipeWireLinkBackend backend(nullptr);
        backend.unsupportedVolumeNodes.insert(9);
        VolumeController volume(&backend);
        QVERIFY(!volume.volumeSupported(9));
        const auto result = volume.setDestinationVolume(9, QStringLiteral("dest-a"), 0.2);
        QVERIFY(!result.allSucceeded);
        QVERIFY(result.error.category == RouteError::VolumeControlUnsupported);
        QCOMPARE(result.failedEndpointIds, QStringList{QStringLiteral("dest-a")});
    }

    void failedWriteIsDistinctFromUnsupported()
    {
        FakePipeWireLinkBackend backend(nullptr);
        backend.failVolumeNodes.insert(3);
        VolumeController volume(&backend);
        const auto result = volume.setDestinationMuted(3, QStringLiteral("dest-a"), true);
        QVERIFY(result.error.category == RouteError::VolumeControlFailed);
    }

    void routeVolumePartialResult()
    {
        FakePipeWireLinkBackend backend(nullptr);
        backend.failVolumeNodes.insert(2);
        VolumeController volume(&backend);
        const auto result = volume.setRouteVolume(
            {{QStringLiteral("dest-a"), 1}, {QStringLiteral("dest-b"), 2}},
            0.3);
        QVERIFY(!result.allSucceeded);
        QVERIFY(result.error.category == RouteError::VolumeControlFailed);
        QCOMPARE(result.failedEndpointIds, QStringList{QStringLiteral("dest-b")});
        QCOMPARE(backend.lastVolumeNode, static_cast<quint32>(1));
        QCOMPARE(backend.lastVolume, 0.3);
    }

    void destinationDelay()
    {
        FakePipeWireLinkBackend backend(nullptr);
        VolumeController volume(&backend);
        const auto result = volume.setDestinationDelayMs(4, QStringLiteral("dest-a"), 80);
        QVERIFY(result.allSucceeded);
        QCOMPARE(backend.lastDelayNode, static_cast<quint32>(4));
        QCOMPARE(backend.lastDelaySeconds, 0.08);
    }
};

QTEST_GUILESS_MAIN(TstVolumeController)
#include "tst_VolumeController.moc"
