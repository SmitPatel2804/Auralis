#include <auralis/audio/AudioEndpointListModel.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/bluetooth/DeviceRegistry.h>

#include <QElapsedTimer>
#include <QtTest>

#include <functional>

using auralis::audio::AudioEndpointListModel;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
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

        BluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        PipeWireManager audio(bluetooth.deviceRegistry());
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
        QVERIFY2(
            waitUntil([&audio]() { return audio.endpointCount() > 0; }, 8000),
            qPrintable(QStringLiteral("No AudioEndpoint objects after live enumeration: %1").arg(diagnostics(audio))));

        auto* model = qobject_cast<AudioEndpointListModel*>(audio.endpoints());
        QVERIFY(model != nullptr);
        QVERIFY(model->rowCount() > 0);

        const QString expectedAddress = QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
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
        bluetooth.shutdown();
        QCOMPARE(audio.endpointCount(), 0);
        QVERIFY(audio.connectionState() == PipeWireConnectionState::Stopped);
    }
};

QTEST_GUILESS_MAIN(TstPipeWireLiveIntegration)
#include "tst_PipeWireLiveIntegration.moc"
