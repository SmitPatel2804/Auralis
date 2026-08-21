#include <auralis/audio/NativeAudioManager.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>

#include <QTest>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QSet>

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
        QVERIFY2(!router->sourceList().isEmpty(), "No native audio source is available");

#if defined(Q_OS_WIN)
        QSet<quint32> processIds;
        for (const auralis::audio::AudioSource& source : router->sourceList()) {
            qInfo() << "application source" << source.applicationName << source.description
                    << "pid=" << source.processId.value_or(0);
            if (source.sourceType == auralis::audio::AudioSourceType::VirtualAudioSource) {
                QVERIFY(source.description.contains(QStringLiteral("Auralis"), Qt::CaseInsensitive));
                QVERIFY(!source.processId.has_value());
                continue;
            }
            QCOMPARE(
                static_cast<int>(source.sourceType),
                static_cast<int>(auralis::audio::AudioSourceType::ApplicationPlaybackStream));
            QVERIFY(source.processId.has_value());
            QVERIFY2(!processIds.contains(*source.processId), "Application audio session was duplicated across render endpoints");
            processIds.insert(*source.processId);
        }
#else
        for (const QAudioDevice& input : QMediaDevices::audioInputs()) {
            const QAudioFormat format = input.preferredFormat();
            qInfo() << "native input" << input.description() << format.sampleRate()
                    << format.channelCount() << format.sampleFormat() << format.channelConfig();
        }
#endif
        for (const QAudioDevice& output : QMediaDevices::audioOutputs()) {
            const QAudioFormat format = output.preferredFormat();
            qInfo() << "native output" << output.description() << format.sampleRate()
                    << format.channelCount() << format.sampleFormat() << format.channelConfig();
        }

        const auto playback = endpoints->playbackEndpoints();
        QVERIFY2(!playback.isEmpty(), "No Windows audio playback destination is available");
        QStringList destinationIds;
#if defined(Q_OS_WIN)
        // Native application capture is a copy of audio that Windows still
        // sends to its default endpoint. Feeding that copy back to the same
        // endpoint would create the delayed double playback this test is
        // intended to guard against. NativeAudioManager sorts the current
        // default first, so exercise every available non-default endpoint.
        for (qsizetype index = 1; index < playback.size(); ++index) {
            destinationIds.push_back(playback.at(index).id);
        }
        if (destinationIds.isEmpty()) {
            QSKIP("Only the Windows default output is available; no safe copy-route destination can be tested");
        }
#else
        for (qsizetype index = 0; index < std::min<qsizetype>(2, playback.size()); ++index) {
            destinationIds.push_back(playback.at(index).id);
        }
#endif

#if defined(Q_OS_WIN)
        const QString sourceId = router->sourceList().constFirst().id;
#else
        const QString defaultInputName = QMediaDevices::defaultAudioInput().description();
        QString sourceId;
        for (const auralis::audio::AudioSource& source : router->sourceList()) {
            if (source.description == defaultInputName) {
                sourceId = source.id;
                break;
            }
        }
#endif
        QVERIFY2(!sourceId.isEmpty(), "The application audio session has no stable source ID");

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

    void liveBluetoothDeviceMapsToNativePlaybackEndpoint()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_NATIVE_DEVICE_MAPPING_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_NATIVE_DEVICE_MAPPING_INTEGRATION=1 to correlate real Bluetooth and audio devices");
        }

        const QString expectedAddress =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        QVERIFY2(!expectedAddress.isEmpty(), "AURALIS_EXPECT_DEVICE_ADDRESS is required for native mapping validation");

        auralis::bluetooth::NativeBluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        QVERIFY(bluetooth.available());
        bluetooth.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth.scanning(), 3000);

        const auto expectedDeviceId = [&bluetooth, &expectedAddress]() {
            for (const auto& device : bluetooth.deviceRegistry()->devices()) {
                if (device.address.trimmed().toUpper() == expectedAddress) {
                    return device.objectPath;
                }
            }
            return QString();
        };
        QTRY_VERIFY_WITH_TIMEOUT(!expectedDeviceId().isEmpty(), 12000);
        bluetooth.stopScan();

        auralis::audio::NativeAudioManager audio(bluetooth.deviceRegistry());
        QVERIFY(audio.initialize());
        QTRY_VERIFY_WITH_TIMEOUT(audio.mappedBluetoothCount() >= 1, 3000);

        bool foundMappedPlayback = false;
        for (const auto& endpoint : audio.endpointRegistry()->playbackEndpoints()) {
            if (endpoint.bluetoothAddress.trimmed().toUpper() != expectedAddress) {
                continue;
            }
            foundMappedPlayback = true;
            QCOMPARE(endpoint.bluetoothDeviceId, expectedDeviceId());
            QVERIFY(!endpoint.bluetoothDisplayName.trimmed().isEmpty());
            QVERIFY(endpoint.name.contains(QStringLiteral("Smokin"), Qt::CaseInsensitive));
        }
        QVERIFY2(foundMappedPlayback, "The expected Bluetooth device was not mapped to a native playback endpoint");
        QCOMPARE(audio.audioStatusForDevice(expectedDeviceId()), QStringLiteral("Available"));

        audio.shutdown();
        bluetooth.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstNativeAudioManager)
#include "tst_NativeAudioManager.moc"
