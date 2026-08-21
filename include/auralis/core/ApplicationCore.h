#pragma once

#include <auralis/audio/IAudioManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/PlatformCapabilities.h>
#include <auralis/core/ServiceStatus.h>
#include <auralis/devices/IDeviceManager.h>
#include <auralis/recovery/RecoveryManager.h>
#include <auralis/recovery/SystemPowerMonitor.h>
#include <auralis/session/ISessionManager.h>
#include <auralis/ui/DiagnosticsLogModel.h>
#include <auralis/ui/NotificationController.h>

#include <QObject>
#include <QString>

#include <memory>

namespace auralis::core {

struct ApplicationServices {
    std::unique_ptr<ConfigurationManager> configuration;
    std::unique_ptr<bluetooth::IBluetoothManager> bluetooth;
    std::unique_ptr<audio::IAudioManager> pipeWire;
    std::unique_ptr<devices::IDeviceManager> devices;
    std::unique_ptr<session::ISessionManager> sessions;
    std::unique_ptr<recovery::RecoveryManager> recovery;
    std::unique_ptr<recovery::SystemPowerMonitor> powerMonitor;
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
    Q_PROPERTY(bool showDeveloperStatus READ showDeveloperStatus NOTIFY showDeveloperStatusChanged)
    Q_PROPERTY(QObject* sessions READ sessions CONSTANT)
    Q_PROPERTY(QObject* configuration READ configuration CONSTANT)
    Q_PROPERTY(QObject* notifications READ notifications CONSTANT)
    Q_PROPERTY(QObject* diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(QObject* recovery READ recovery CONSTANT)
    Q_PROPERTY(QObject* platform READ platform CONSTANT)
    Q_PROPERTY(QString recoveryStatus READ recoveryStatus NOTIFY recoveryStatusChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY warningCountChanged)
    Q_PROPERTY(QString lastErrorText READ lastErrorText NOTIFY lastErrorTextChanged)
    Q_PROPERTY(QString activeSessionName READ activeSessionName NOTIFY activeSessionChanged)
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)

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
    QObject* sessions() const;
    QObject* configuration() const;
    QObject* notifications() const;
    QObject* diagnostics() const;
    QObject* recovery() const;
    QObject* platform() const;
    recovery::RecoveryManager* recoveryManager() const noexcept;
    recovery::SystemPowerMonitor* powerMonitor() const noexcept;
    bluetooth::IBluetoothManager* bluetoothService() const noexcept;
    audio::IAudioManager* audioService() const noexcept;
    QString recoveryStatus() const;
    int currentPage() const;
    void setCurrentPage(int page);
    int warningCount() const;
    QString lastErrorText() const;
    QString activeSessionName() const;
    QString activeSessionId() const;

    Q_INVOKABLE void navigateTo(int page);

signals:
    void statusChanged();
    void showDeveloperStatusChanged();
    void currentPageChanged();
    void warningCountChanged();
    void lastErrorTextChanged();
    void activeSessionChanged();
    void recoveryStatusChanged();

private slots:
    void refreshActiveSessionCache();

private:
    void setStatus(ServiceStatus status);
    void rollbackInitializedServices();

    ApplicationServices services_;
    ServiceStatus status_ = ServiceStatus::Uninitialized;
    bool showDeveloperStatus_ = true;
    int currentPage_ = 0;
    QString activeSessionName_;
    QString activeSessionId_;
    ui::NotificationController notifications_;
    ui::DiagnosticsLogModel diagnostics_;
    PlatformCapabilities platform_;
};

} // namespace auralis::core
