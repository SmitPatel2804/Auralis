#include <auralis/core/PlatformCapabilities.h>

#include <auralis/audio/IAudioManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>

#include <QtGlobal>

namespace auralis::core {

PlatformCapabilities::PlatformCapabilities(QObject* parent)
    : QObject(parent)
{
}

QString PlatformCapabilities::operatingSystem() const
{
#if defined(Q_OS_LINUX)
    return QStringLiteral("Linux");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS");
#else
    return QStringLiteral("Unsupported desktop OS");
#endif
}

QString PlatformCapabilities::bluetoothBackend() const
{
    return bluetoothBackend_;
}

QString PlatformCapabilities::audioBackend() const
{
    return audioBackend_;
}

QString PlatformCapabilities::powerBackend() const
{
#if defined(Q_OS_LINUX)
    return QStringLiteral("systemd-logind");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Windows power notifications");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("NSWorkspace");
#else
    return QStringLiteral("Unknown");
#endif
}

bool PlatformCapabilities::desktop() const noexcept
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

void PlatformCapabilities::configure(
    const bluetooth::IBluetoothManager* bluetooth,
    const audio::IAudioManager* audio)
{
    const QString nextBluetooth = bluetooth != nullptr
        ? bluetooth->backendName()
        : QStringLiteral("Unavailable");
    const QString nextAudio = audio != nullptr
        ? audio->backendName()
        : QStringLiteral("Unavailable");
    if (nextBluetooth == bluetoothBackend_ && nextAudio == audioBackend_) {
        return;
    }
    bluetoothBackend_ = nextBluetooth;
    audioBackend_ = nextAudio;
    emit backendsChanged();
}

} // namespace auralis::core
