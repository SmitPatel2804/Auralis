#pragma once

#include <auralis/bluetooth/IBlueZClient.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <optional>

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

    /// Test seam: inject system-bus connectivity without stopping host dbus.
    void injectSystemBusConnectedForTesting(bool connected);
    /// Override bus probe used by the health timer (exercises production FSM without host dbus).
    void setSystemBusConnectedOverrideForTesting(std::optional<bool> connected);
    void pollSystemBusHealthForTesting();
    /// Deliver a GetManagedObjects completion for a captured attach generation.
    void injectSnapshotFinishedForTesting(quint64 generation, const QVariantMap& objects, bool error = false);
    void setBlueZAvailableForTesting(bool available);
    int busAttachGenerationForTesting() const noexcept;
    bool busHealthTimerActiveForTesting() const noexcept;
    bool snapshotInFlightForTesting() const noexcept;

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
    void pollSystemBusHealth();

private:
    bool probeSystemBusConnected() const;
    void subscribeToSignals();
    void unsubscribeFromSignals();
    void setBlueZAvailable(bool available);
    void setSystemBusConnected(bool connected);
    void detachSystemBusInfrastructure();
    bool attachSystemBusInfrastructure();
    void handleSystemBusLost();
    void attemptSystemBusReattach();
    void startBusHealthTimer();
    void stopBusHealthTimer();
    void finishSnapshot(quint64 generation, const QVariantMap& objects, bool error, const QString& name, const QString& message);
    void issueSnapshotCall();

    QDBusConnection connection_;
    QDBusServiceWatcher* serviceWatcher_ = nullptr;
    QTimer busHealthTimer_;
    bool initialized_ = false;
    bool systemBusConnected_ = false;
    bool blueZAvailable_ = false;
    bool signalsSubscribed_ = false;
    bool agentRegistered_ = false;
    bool snapshotInFlight_ = false;
    bool pendingSnapshotRefresh_ = false;
    quint64 busAttachGeneration_ = 0;
    quint64 snapshotInFlightGeneration_ = 0;
    std::optional<bool> systemBusConnectedOverride_;
};

} // namespace auralis::bluetooth
