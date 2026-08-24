#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireVirtualOutput.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>

#include <QElapsedTimer>
#include <QtTest>

#include <functional>

using auralis::audio::AudioEndpointListModel;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::audio::inspectPipeWireVirtualOutput;
using auralis::audio::kAuralisVirtualSourceId;
using auralis::bluetooth::BluetoothManager;
using auralis::bluetooth::DeviceRegistry;

class TstPipeWireLiveIntegration : public QObject {
    Q_OBJECT

private:
    static QString diagnostics(const PipeWireManager& audio)
    {
        return audio.diagnosticsText();
    }

    static bool waitUntil(const std::function<bool()>& predicate, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (predicate()) {
                return true;
            }
            QTest::qWait(50);
        }
        return predicate();
    }

private slots:
    void livePipeWireRoundTrip()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_PIPEWIRE_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_PIPEWIRE_INTEGRATION=1 to run live PipeWire tests");
        }

        const QString expectedAddress =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        BluetoothManager bluetooth;
        if (!expectedAddress.isEmpty()) {
            QVERIFY2(
                bluetooth.initialize(),
                "Bluetooth must initialize when AURALIS_EXPECT_DEVICE_ADDRESS is set");
        }
        PipeWireManager audio(expectedAddress.isEmpty() ? nullptr : bluetooth.deviceRegistry());
        QVERIFY(audio.initialize());
        QVERIFY2(
            waitUntil(
                [&audio]() {
                    return audio.connectionState() == PipeWireConnectionState::Connected
                        || audio.connectionState() == PipeWireConnectionState::Error;
                },
                5000),
            qPrintable(QStringLiteral("PipeWire did not settle: %1").arg(diagnostics(audio))));
        QVERIFY2(
            audio.connectionState() == PipeWireConnectionState::Connected,
            qPrintable(QStringLiteral("AURALIS_RUN_PIPEWIRE_INTEGRATION=1 but PipeWire is not connected: %1")
                           .arg(diagnostics(audio))));
        QVERIFY2(
            waitUntil([&audio]() { return audio.initialSyncComplete(); }, 5000),
            qPrintable(QStringLiteral("Initial registry sync did not complete: %1").arg(diagnostics(audio))));
        auto* model = qobject_cast<AudioEndpointListModel*>(audio.endpoints());
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), audio.endpointCount());

        QVERIFY2(
            waitUntil([&audio]() { return audio.virtualOutputAvailable(); }, 8000),
            qPrintable(QStringLiteral("Auralis virtual output did not become ready: %1").arg(diagnostics(audio))));
        const auto* store = audio.objectStore();
        QVERIFY(store != nullptr);
        const auto virtualState = inspectPipeWireVirtualOutput(*store);
        const QString expectedPersistence =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_VIRTUAL_OUTPUT_PERSISTENCE")).trimmed();
        // Isolated CI PipeWire has no other sink, so WirePlumber often selects
        // the virtual output as default. The UI status then becomes "Selected
        // as system output" and no longer mentions persistence.
        if (expectedPersistence == QLatin1String("persistent")) {
            QVERIFY2(
                virtualState.packageManaged,
                qPrintable(QStringLiteral("Expected package-managed virtual output: %1").arg(diagnostics(audio))));
        } else if (expectedPersistence == QLatin1String("runtime")) {
            QVERIFY2(
                virtualState.runtimeManaged && !virtualState.packageManaged,
                qPrintable(QStringLiteral("Expected runtime-managed virtual output: %1").arg(diagnostics(audio))));
        }
        auto* router = audio.audioRouter();
        QVERIFY(router != nullptr);
        QVERIFY2(
            waitUntil(
                [router]() {
                    for (const auto& source : router->sourceList()) {
                        if (source.id == QLatin1String(kAuralisVirtualSourceId)) {
                            return true;
                        }
                    }
                    return false;
                },
                3000),
            qPrintable(QStringLiteral("Auralis system-audio source missing: %1").arg(diagnostics(audio))));

        for (const auto& endpoint : audio.endpointRegistry()->endpoints()) {
            QVERIFY2(
                endpoint.nodeName != QLatin1String("auralis_virtual_output"),
                "The Auralis capture sink must not be offered as a route destination");
        }

        if (!expectedAddress.isEmpty()) {
            const bool mapped = waitUntil(
                [&audio, expectedAddress]() {
                    const auto endpoints = audio.endpointRegistry()->mappedBluetoothEndpoints();
                    for (const auto& endpoint : endpoints) {
                        if (endpoint.bluetoothAddress.toUpper() == expectedAddress) {
                            return true;
                        }
                    }
                    return false;
                },
                12000);
            QVERIFY2(
                mapped,
                qPrintable(QStringLiteral("Expected Bluetooth mapping for %1 never appeared. %2")
                               .arg(expectedAddress, diagnostics(audio))));
        }

        audio.shutdown();
        if (!expectedAddress.isEmpty()) {
            bluetooth.shutdown();
        }
        QCOMPARE(audio.endpointCount(), 0);
        QVERIFY(audio.connectionState() == PipeWireConnectionState::Stopped);
    }
};

QTEST_GUILESS_MAIN(TstPipeWireLiveIntegration)
#include "tst_PipeWireLiveIntegration.moc"
