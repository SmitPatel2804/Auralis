#pragma once

#include <auralis/bluetooth/BluetoothError.h>

#include <QObject>
#include <QString>

namespace auralis::bluetooth {

class AdapterManager;
class IBlueZClient;

enum class DiscoveryState {
    Unavailable,
    Idle,
    Starting,
    Discovering,
    Stopping,
    Error
};

QString toString(DiscoveryState state);

class DiscoveryManager final : public QObject {
    Q_OBJECT

public:
    DiscoveryManager(IBlueZClient* client, AdapterManager* adapters, QObject* parent = nullptr);

    void startScan();
    void stopScan();
    void shutdown();
    void onBlueZAvailabilityChanged(bool available);
    void onSelectedAdapterChanged();
    void onAdapterDiscoveringPropertyChanged(bool discovering);
    void handleStartFinished(
        const QString& adapterPath,
        bool succeeded,
        const QString& errorName,
        const QString& errorMessage);
    void handleStopFinished(
        const QString& adapterPath,
        bool succeeded,
        const QString& errorName,
        const QString& errorMessage);

    DiscoveryState state() const noexcept;
    bool ownsDiscovery() const noexcept;
    bool desiredScanning() const noexcept;
    bool canStartScan() const;
    bool canStopScan() const;
    BluetoothError lastError() const noexcept;
    QString lastErrorMessage() const;
    QString statusText() const;

signals:
    void stateChanged();
    void errorChanged();
    void intentChanged();

private:
    enum class PendingOperation {
        None,
        Start,
        Stop
    };

    bool prerequisitesValid() const;
    bool isStaleCallback(const QString& adapterPath) const;
    void reconcilePrerequisites();
    void reconcileDesiredState();
    void beginStart();
    void beginStop();
    void dropSession(DiscoveryState next);
    void applyUnexpectedDiscoveryStop();
    void clearRecoverablePrerequisiteError();
    void setDesiredScanning(bool desired);
    void setPendingOperation(PendingOperation operation);
    void setState(DiscoveryState state);
    void setError(BluetoothError error, const QString& message = {});

    IBlueZClient* client_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    DiscoveryState state_ = DiscoveryState::Unavailable;
    bool desiredScanning_ = false;
    bool ownsDiscovery_ = false;
    bool confirmedAdapterDiscovering_ = false;
    bool shuttingDown_ = false;
    bool lastBlueZAvailable_ = false;
    PendingOperation pendingOperation_ = PendingOperation::None;
    BluetoothError lastError_ = BluetoothError::None;
    QString lastErrorMessage_;
    QString operationAdapterPath_;
    quint64 bluezGeneration_ = 0;
    quint64 operationGeneration_ = 0;
};

} // namespace auralis::bluetooth
