#include <auralis/audio/NativeAudioManager.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/DeviceRegistry.h>

#include <QTest>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>

#include <algorithm>

class TstNativeAudioManager final : public QObject {
    Q_OBJECT

private slots:
    void volatileBluetoothUpdatesDoNotRebuildAudioGraph()
    {
        auralis::bluetooth::DeviceRegistry registry;
        auralis::audio::NativeAudioManager manager(&registry);
        QVERIFY(manager.initialize());

        auralis::bluetooth::BluetoothDeviceData device;
        device.objectPath = QStringLiteral("native:test-device");
        device.address = QStringLiteral("00:11:22:33:44:55");
        device.name = QStringLiteral("Test Headphones");
        registry.upsertDevice(device);
        QTRY_VERIFY_WITH_TIMEOUT(manager.graphRevision() >= 2, 1000);

        const int stableRevision = manager.graphRevision();
        QVERIFY(registry.setReconnectAttempt(device.objectPath, 1));
        QTest::qWait(450);
        QCOMPARE(manager.graphRevision(), stableRevision);

        manager.shutdown();
    }

    void mutedNativeRouteRoundTrip()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_NATIVE_AUDIO_ROUTING_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_NATIVE_AUDIO_ROUTING_INTEGRATION=1 to open real native audio devices");
        }

        auralis::bluetooth::DeviceRegistry registry;
        auralis::audio::NativeAudioManager manager(&registry);
        QVERIFY(manager.initialize());

        auto* router = manager.audioRouter();
        auto* endpoints = manager.endpointRegistry();
        QVERIFY(router != nullptr);
        QVERIFY(endpoints != nullptr);
        QVERIFY2(!router->sourceList().isEmpty(), "No Windows audio capture source is available");

        for (const QAudioDevice& input : QMediaDevices::audioInputs()) {
            const QAudioFormat format = input.preferredFormat();
            qInfo() << "native input" << input.description() << format.sampleRate()
                    << format.channelCount() << format.sampleFormat() << format.channelConfig();
        }
        for (const QAudioDevice& output : QMediaDevices::audioOutputs()) {
            const QAudioFormat format = output.preferredFormat();
            qInfo() << "native output" << output.description() << format.sampleRate()
                    << format.channelCount() << format.sampleFormat() << format.channelConfig();
        }

        const auto playback = endpoints->playbackEndpoints();
        QVERIFY2(!playback.isEmpty(), "No Windows audio playback destination is available");
        QStringList destinationIds;
        for (qsizetype index = 0; index < std::min<qsizetype>(2, playback.size()); ++index) {
            destinationIds.push_back(playback.at(index).id);
        }

        const QString defaultInputName = QMediaDevices::defaultAudioInput().description();
        QCOMPARE(router->sourceList().constFirst().description, defaultInputName);
        QString sourceId;
        for (const auralis::audio::AudioSource& source : router->sourceList()) {
            if (source.description == defaultInputName) {
                sourceId = source.id;
                break;
            }
        }
        QVERIFY2(!sourceId.isEmpty(), "The default Windows capture input was not projected into the audio graph");

        const QString routeId = router->createRoute(sourceId, destinationIds);
        QVERIFY2(!routeId.isEmpty(), qPrintable(router->lastErrorText()));
        router->setRouteMuted(routeId, true);
        for (int cycle = 0; cycle < 5; ++cycle) {
            router->activateRoute(routeId);
            QTRY_VERIFY_WITH_TIMEOUT(
                router->routeById(routeId).has_value()
                    && router->routeById(routeId)->state == auralis::audio::RouteState::Active,
                5000);
            QCOMPARE(router->ownedLinkCount(), destinationIds.size());

            QTest::qWait(150);
            router->deactivateRoute(routeId);
            QTRY_VERIFY_WITH_TIMEOUT(
                router->routeById(routeId).has_value()
                    && router->routeById(routeId)->state == auralis::audio::RouteState::Inactive,
                2000);
            QCOMPARE(router->ownedLinkCount(), 0);
        }
        QVERIFY(manager.lastError().isEmpty());
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstNativeAudioManager)
#include "tst_NativeAudioManager.moc"
