#pragma once

#include <QObject>
#include <QString>

namespace auralis::audio {
class IAudioManager;
}

namespace auralis::bluetooth {
class IBluetoothManager;
}

namespace auralis::core {

/// Describes the selected native backend set without leaking platform checks
/// into QML or the shared domain/session layers.
class PlatformCapabilities final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString operatingSystem READ operatingSystem CONSTANT)
    Q_PROPERTY(QString bluetoothBackend READ bluetoothBackend NOTIFY backendsChanged)
    Q_PROPERTY(QString audioBackend READ audioBackend NOTIFY backendsChanged)
    Q_PROPERTY(QString powerBackend READ powerBackend CONSTANT)
    Q_PROPERTY(bool desktop READ desktop CONSTANT)

public:
    explicit PlatformCapabilities(QObject* parent = nullptr);

    QString operatingSystem() const;
    QString bluetoothBackend() const;
    QString audioBackend() const;
    QString powerBackend() const;
    bool desktop() const noexcept;

    void configure(
        const bluetooth::IBluetoothManager* bluetooth,
        const audio::IAudioManager* audio);

signals:
    void backendsChanged();

private:
    QString bluetoothBackend_{QStringLiteral("Unknown")};
    QString audioBackend_{QStringLiteral("Unknown")};
};

} // namespace auralis::core
