#pragma once

#include <auralis/bluetooth/IBlueZClient.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QString>
#include <QVariantMap>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace auralis::bluetooth {

class BlueZDbusClient final : public IBlueZClient {
    Q_OBJECT

public:
    explicit BlueZDbusClient(QObject* parent = nullptr);
    ~BlueZDbusClient() override;

    bool initialize() override;
    void shutdown() override;
    bool isSystemBusConnected() const noexcept override;
    bool isBlueZAvailable() const noexcept override;

    void requestSnapshot() override;
    void startDiscovery(const QString& adapterPath) override;
    void stopDiscovery(const QString& adapterPath) override;

    void pairDevice(const QString& devicePath) override;
    void cancelPairing(const QString& devicePath) override;
    void connectDevice(const QString& devicePath) override;
    void disconnectDevice(const QString& devicePath) override;
    void setDeviceTrusted(const QString& devicePath, bool trusted) override;
    void removeDevice(const QString& adapterPath, const QString& devicePath) override;

    void registerAgent(const QString& agentPath, const QString& capability) override;
    void requestDefaultAgent(const QString& agentPath) override;
    void unregisterAgent(const QString& agentPath) override;
    bool isAgentRegistered() const noexcept override;

private slots:
    void onBlueZRegistered(const QString& serviceName);
    void onBlueZUnregistered(const QString& serviceName);
    void onGetManagedObjectsFinished(QDBusPendingCallWatcher* watcher);
    void onInterfacesAdded(const QDBusObjectPath& objectPath, const QDBusMessage& message);
    void onInterfacesRemoved(const QDBusObjectPath& objectPath, const QStringList& interfaces);
    void onPropertiesChanged(
        const QString& interfaceName,
        const QVariantMap& changed,
        const QStringList& invalidated,
        const QDBusMessage& message);

private:
    void subscribeToSignals();
    void unsubscribeFromSignals();
    void setBlueZAvailable(bool available);

    QDBusConnection connection_;
    QDBusServiceWatcher* serviceWatcher_ = nullptr;
    bool initialized_ = false;
    bool systemBusConnected_ = false;
    bool blueZAvailable_ = false;
    bool signalsSubscribed_ = false;
    bool agentRegistered_ = false;
};

} // namespace auralis::bluetooth
