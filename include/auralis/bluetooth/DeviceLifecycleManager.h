#pragma once

#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/DeviceOperation.h>
#include <auralis/bluetooth/DeviceReconnectMetadata.h>

#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <QObject>
#include <QString>
#include <QTimer>

namespace auralis::bluetooth {

class AdapterManager;
class BlueZAgent;
class DeviceRegistry;
class DiscoveryManager;
class IBlueZClient;
class ReconnectPolicy;

class DeviceLifecycleManager final : public QObject {
    Q_OBJECT

public:
    DeviceLifecycleManager(
        IBlueZClient* client,
        DeviceRegistry* registry,
        AdapterManager* adapters,
        BlueZAgent* agent,
        ReconnectPolicy* reconnect,
        DiscoveryManager* discovery = nullptr,
        QObject* parent = nullptr);

    void shutdown();
    void onBlueZAvailabilityChanged(bool available);
    void onSnapshotApplied();
    void applyStoredMetadataToRegistry();
    void onDeviceRemoved(const QString& objectPath);
    void onDevicePropertiesChanged(
        const QString& objectPath,
        const QVariantMap& changed,
        const QStringList& invalidated);

    void pairDevice(const QString& objectPath);
    void cancelPairing(const QString& objectPath);
    void trustDevice(const QString& objectPath);
    void untrustDevice(const QString& objectPath);
    void connectDevice(const QString& objectPath);
    void disconnectDevice(const QString& objectPath);
    void forgetDevice(const QString& objectPath);
    void reconnectDevice(const QString& objectPath);
    void cancelDeviceOperation(const QString& objectPath);

    void suppressAutoReconnect(const QString& objectPath);
    void unsuppressAutoReconnect(const QString& objectPath);
    bool isAutoReconnectSuppressed(const QString& objectPath) const;

signals:
    void deviceOperationChanged(const QString& objectPath);

private:
    struct PendingOp {
        DeviceOperation operation = DeviceOperation::Idle;
        quint64 generation = 0;
        bool waitingProperty = false;
        bool expectedTrusted = false;
        QTimer* timeoutTimer = nullptr;
    };

    bool beginOperation(const QString& objectPath, DeviceOperation operation);
    bool transitionOperation(const QString& objectPath, DeviceOperation from, DeviceOperation to);
    void finishOperation(const QString& objectPath, quint64 generation, bool success, BluetoothError error, const QString& errorName, const QString& message);
    void abortPendingOperation(const QString& objectPath, const QString& reason, BluetoothError error = BluetoothError::OperationFailed);
    void clearPending(const QString& objectPath);
    void stopOperationTimeout(const QString& objectPath);
    void startOperationTimeout(const QString& objectPath, DeviceOperation operation);
    int operationTimeoutMs(DeviceOperation operation) const;
    quint64 bumpGeneration(const QString& objectPath);
    bool isStale(const QString& objectPath, quint64 generation) const;
    const BluetoothDeviceData* requireDevice(const QString& objectPath) const;
    void handleUnexpectedDisconnect(const QString& objectPath, const BluetoothDeviceData& device);
    void reevaluateReconnectCandidates();
    void syncReconnectMetadataFromDevice(const BluetoothDeviceData& device);
    void removeReconnectMetadata(const BluetoothDeviceData& device);
    void setUserDisconnectRequested(const QString& objectPath, bool requested);
    void clearUserDisconnectSuppression(const QString& objectPath);
    void syncDiscoveryHold();
    bool shouldHoldDiscovery() const;

    void handlePairFinished(const QString& devicePath, bool succeeded, const QString& errorName, const QString& errorMessage);
    void handleCancelPairingFinished(const QString& devicePath, bool succeeded, const QString& errorName, const QString& errorMessage);
    void handleConnectFinished(const QString& devicePath, bool succeeded, const QString& errorName, const QString& errorMessage);
    void handleDisconnectFinished(const QString& devicePath, bool succeeded, const QString& errorName, const QString& errorMessage);
    void handleTrustFinished(const QString& devicePath, bool trusted, bool succeeded, const QString& errorName, const QString& errorMessage);
    void handleRemoveFinished(const QString& adapterPath, const QString& devicePath, bool succeeded, const QString& errorName, const QString& errorMessage);
    void checkPropertyCompletion(const QString& objectPath, const BluetoothDeviceData& device);

    IBlueZClient* client_ = nullptr;
    DeviceRegistry* registry_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    BlueZAgent* agent_ = nullptr;
    ReconnectPolicy* reconnect_ = nullptr;
    DiscoveryManager* discovery_ = nullptr;
    QHash<QString, PendingOp> pending_;
    QHash<QString, quint64> generations_;
    QHash<QString, DeviceReconnectMetadata> reconnectMetadata_;
    QSet<QString> autoReconnectSuppressed_;
};

} // namespace auralis::bluetooth
