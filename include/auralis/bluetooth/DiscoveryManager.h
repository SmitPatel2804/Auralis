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
    void onBlueZAvailabilityChanged(bool available);
    void onSelectedAdapterChanged();
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
    bool canStartScan() const;
    bool canStopScan() const;
    BluetoothError lastError() const noexcept;
    QString lastErrorMessage() const;
    QString statusText() const;

signals:
    void stateChanged();
    void errorChanged();

private:
    void setState(DiscoveryState state);
    void setError(BluetoothError error, const QString& message = {});
    void dropOwnership(DiscoveryState next);

    IBlueZClient* client_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    DiscoveryState state_ = DiscoveryState::Unavailable;
    bool ownsDiscovery_ = false;
    BluetoothError lastError_ = BluetoothError::None;
    QString lastErrorMessage_;
    QString pendingAdapterPath_;
};

} // namespace auralis::bluetooth
