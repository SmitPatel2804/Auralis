#pragma once

#include <auralis/audio/IPipeWireManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/ServiceStatus.h>
#include <auralis/devices/IDeviceManager.h>
#include <auralis/session/ISessionManager.h>

#include <QObject>
#include <QString>

#include <memory>

namespace auralis::core {

struct ApplicationServices {
    std::unique_ptr<ConfigurationManager> configuration;
    std::unique_ptr<bluetooth::IBluetoothManager> bluetooth;
    std::unique_ptr<audio::IPipeWireManager> pipeWire;
    std::unique_ptr<devices::IDeviceManager> devices;
    std::unique_ptr<session::ISessionManager> sessions;
};

class ApplicationCore final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY statusChanged)
    Q_PROPERTY(QObject* bluetooth READ bluetooth CONSTANT)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY statusChanged)
    Q_PROPERTY(QObject* audio READ audio CONSTANT)
    Q_PROPERTY(QString pipeWireStatus READ pipeWireStatus NOTIFY statusChanged)
    Q_PROPERTY(QString coreStatus READ coreStatus NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY statusChanged)
    Q_PROPERTY(bool showDeveloperStatus READ showDeveloperStatus NOTIFY statusChanged)

public:
    explicit ApplicationCore(ApplicationServices services, QObject* parent = nullptr);
    ~ApplicationCore() override;

    ApplicationCore(const ApplicationCore&) = delete;
    ApplicationCore& operator=(const ApplicationCore&) = delete;

    bool initialize();
    void shutdown();

    bool isReady() const noexcept;
    bool showDeveloperStatus() const;

    QString bluetoothStatus() const;
    QObject* bluetooth() const;
    QString audioStatus() const;
    QObject* audio() const;
    QString pipeWireStatus() const;
    QString coreStatus() const;

signals:
    void statusChanged();

private:
    void setStatus(ServiceStatus status);
    void rollbackInitializedServices();

    ApplicationServices services_;
    ServiceStatus status_ = ServiceStatus::Uninitialized;
    bool showDeveloperStatus_ = true;
};

} // namespace auralis::core
