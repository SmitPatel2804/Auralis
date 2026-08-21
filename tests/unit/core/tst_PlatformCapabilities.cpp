#include <auralis/audio/IAudioManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/core/PlatformCapabilities.h>

#include <QtTest>

namespace {

class TestBluetooth final : public auralis::bluetooth::IBluetoothManager {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    auralis::core::ServiceStatus status() const noexcept override
    {
        return auralis::core::ServiceStatus::Ready;
    }
    QString backendName() const override { return QStringLiteral("Test Bluetooth"); }
};

class TestAudio final : public auralis::audio::IAudioManager {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    auralis::core::ServiceStatus status() const noexcept override
    {
        return auralis::core::ServiceStatus::Ready;
    }
    QString backendName() const override { return QStringLiteral("Test Audio"); }
};

} // namespace

class TestPlatformCapabilities final : public QObject {
    Q_OBJECT

private slots:
    void identifiesSupportedDesktopFamily()
    {
        auralis::core::PlatformCapabilities capabilities;
        QVERIFY(capabilities.desktop());
#if defined(Q_OS_LINUX)
        QCOMPARE(capabilities.operatingSystem(), QStringLiteral("Linux"));
#elif defined(Q_OS_WIN)
        QCOMPARE(capabilities.operatingSystem(), QStringLiteral("Windows"));
#elif defined(Q_OS_MACOS)
        QCOMPARE(capabilities.operatingSystem(), QStringLiteral("macOS"));
#endif
    }

    void reportsSelectedBackendsWithoutPlatformChecksInCallers()
    {
        auralis::core::PlatformCapabilities capabilities;
        TestBluetooth bluetooth;
        TestAudio audio;
        QSignalSpy changed(&capabilities, &auralis::core::PlatformCapabilities::backendsChanged);

        capabilities.configure(&bluetooth, &audio);

        QCOMPARE(capabilities.bluetoothBackend(), QStringLiteral("Test Bluetooth"));
        QCOMPARE(capabilities.audioBackend(), QStringLiteral("Test Audio"));
        QCOMPARE(changed.count(), 1);
        capabilities.configure(&bluetooth, &audio);
        QCOMPARE(changed.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPlatformCapabilities)
#include "tst_PlatformCapabilities.moc"
