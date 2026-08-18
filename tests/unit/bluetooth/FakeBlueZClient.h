#pragma once

#include <auralis/bluetooth/IBlueZClient.h>

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace auralis::test {

class FakeBlueZClient final : public auralis::bluetooth::IBlueZClient {
public:
    explicit FakeBlueZClient(QObject* parent = nullptr)
        : IBlueZClient(parent)
    {
    }

    bool initialize() override
    {
        initialized_ = true;
        emit systemBusStateChanged(systemBusConnected_);
        emit blueZAvailableChanged(blueZAvailable_);
        if (blueZAvailable_) {
            requestSnapshot();
        }
        return true;
    }

    void shutdown() override
    {
        initialized_ = false;
    }

    bool isSystemBusConnected() const noexcept override
    {
        return systemBusConnected_;
    }

    bool isBlueZAvailable() const noexcept override
    {
        return blueZAvailable_;
    }

    void requestSnapshot() override
    {
        ++snapshotRequests_;
        if (!blueZAvailable_) {
            emit snapshotFailed(QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"), QStringLiteral("BlueZ missing"));
            return;
        }
        if (!snapshotSucceeds_) {
            emit snapshotFailed(snapshotErrorName_, snapshotErrorMessage_);
            return;
        }
        emit snapshotReceived(objects_);
    }

    void startDiscovery(const QString& adapterPath) override
    {
        ++startRequests_;
        lastStartPath_ = adapterPath;
        pendingStartPath_ = adapterPath;
        if (!autoCompleteStart_) {
            return;
        }
        emitStartFinished();
    }

    void stopDiscovery(const QString& adapterPath) override
    {
        ++stopRequests_;
        lastStopPath_ = adapterPath;
        pendingStopPath_ = adapterPath;
        if (!autoCompleteStop_) {
            return;
        }
        emitStopFinished();
    }

    void pairDevice(const QString& devicePath) override
    {
        ++pairRequests_;
        lastPairPath_ = devicePath;
        if (!autoCompleteDeviceOps_) {
            return;
        }
        emit pairDeviceFinished(devicePath, pairSucceeds_, pairSucceeds_ ? QString() : pairErrorName_, pairSucceeds_ ? QString() : pairErrorMessage_);
    }

    void cancelPairing(const QString& devicePath) override
    {
        ++cancelPairRequests_;
        lastCancelPairPath_ = devicePath;
        if (!autoCompleteCancelPairing_) {
            return;
        }
        emit cancelPairingFinished(
            devicePath,
            cancelPairingSucceeds_,
            cancelPairingSucceeds_ ? QString() : cancelPairingErrorName_,
            cancelPairingSucceeds_ ? QString() : cancelPairingErrorMessage_);
    }

    void connectDevice(const QString& devicePath) override
    {
        ++connectRequests_;
        lastConnectPath_ = devicePath;
        if (!autoCompleteDeviceOps_) {
            return;
        }
        emit connectDeviceFinished(devicePath, connectSucceeds_, connectSucceeds_ ? QString() : connectErrorName_, connectSucceeds_ ? QString() : connectErrorMessage_);
    }

    void disconnectDevice(const QString& devicePath) override
    {
        ++disconnectRequests_;
        emit disconnectDeviceFinished(devicePath, true, {}, {});
    }

    void setDeviceTrusted(const QString& devicePath, bool trusted) override
    {
        ++trustRequests_;
        emit setDeviceTrustedFinished(devicePath, trusted, true, {}, {});
    }

    void removeDevice(const QString& adapterPath, const QString& devicePath) override
    {
        ++removeRequests_;
        lastRemoveAdapterPath_ = adapterPath;
        lastRemoveDevicePath_ = devicePath;
        if (!autoCompleteDeviceOps_) {
            return;
        }
        objects_.remove(devicePath);
        emit removeDeviceFinished(adapterPath, devicePath, true, {}, {});
        emit interfacesRemoved(devicePath, QStringList{QStringLiteral("org.bluez.Device1")});
    }

    void registerAgent(const QString& agentPath, const QString& capability) override
    {
        ++registerAgentRequests_;
        lastRegisterAgentPath_ = agentPath;
        lastRegisterAgentCapability_ = capability;
        agentRegistered_ = true;
        emit registerAgentFinished(true, {}, {});
    }

    void unregisterAgent(const QString& agentPath) override
    {
        Q_UNUSED(agentPath);
        agentRegistered_ = false;
        emit unregisterAgentFinished(true, {}, {});
    }

    bool isAgentRegistered() const noexcept override
    {
        return agentRegistered_;
    }

    void setAutoCompleteDeviceOps(bool enabled) { autoCompleteDeviceOps_ = enabled; }
    void setAutoCompleteCancelPairing(bool enabled) { autoCompleteCancelPairing_ = enabled; }
    void setPairResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        pairSucceeds_ = succeeds;
        pairErrorName_ = errorName;
        pairErrorMessage_ = errorMessage;
    }
    void setCancelPairingResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        cancelPairingSucceeds_ = succeeds;
        cancelPairingErrorName_ = errorName;
        cancelPairingErrorMessage_ = errorMessage;
    }
    void setConnectResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        connectSucceeds_ = succeeds;
        connectErrorName_ = errorName;
        connectErrorMessage_ = errorMessage;
    }
    void completePairSuccess(const QString& devicePath)
    {
        emit pairDeviceFinished(devicePath, true, {}, {});
    }
    void completeConnectSuccess(const QString& devicePath)
    {
        emit connectDeviceFinished(devicePath, true, {}, {});
    }
    void completeCancelPairing(const QString& devicePath)
    {
        emit cancelPairingFinished(
            devicePath,
            cancelPairingSucceeds_,
            cancelPairingSucceeds_ ? QString() : cancelPairingErrorName_,
            cancelPairingSucceeds_ ? QString() : cancelPairingErrorMessage_);
    }

    int pairRequests() const { return pairRequests_; }
    int cancelPairRequests() const { return cancelPairRequests_; }
    int connectRequests() const { return connectRequests_; }
    int disconnectRequests() const { return disconnectRequests_; }
    int removeRequests() const { return removeRequests_; }
    int registerAgentRequests() const { return registerAgentRequests_; }
    QString lastRegisterAgentPath() const { return lastRegisterAgentPath_; }
    QString lastRegisterAgentCapability() const { return lastRegisterAgentCapability_; }
    QString lastPairPath() const { return lastPairPath_; }
    QString lastCancelPairPath() const { return lastCancelPairPath_; }
    QString lastConnectPath() const { return lastConnectPath_; }

    void setSystemBusConnected(bool connected)
    {
        systemBusConnected_ = connected;
        emit systemBusStateChanged(connected);
    }

    void setBlueZAvailable(bool available)
    {
        blueZAvailable_ = available;
        emit blueZAvailableChanged(available);
    }

    void setStartResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        startSucceeds_ = succeeds;
        startErrorName_ = errorName;
        startErrorMessage_ = errorMessage;
    }

    void setStopResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        stopSucceeds_ = succeeds;
        stopErrorName_ = errorName;
        stopErrorMessage_ = errorMessage;
    }

    void setSnapshotResult(bool succeeds, const QString& errorName = {}, const QString& errorMessage = {})
    {
        snapshotSucceeds_ = succeeds;
        snapshotErrorName_ = errorName;
        snapshotErrorMessage_ = errorMessage;
    }

    void setAutoCompleteStart(bool enabled)
    {
        autoCompleteStart_ = enabled;
    }

    void setAutoCompleteStop(bool enabled)
    {
        autoCompleteStop_ = enabled;
    }

    void completeStartSuccess()
    {
        startSucceeds_ = true;
        emitStartFinished();
    }

    void completeStartFailure(const QString& errorName, const QString& errorMessage)
    {
        startSucceeds_ = false;
        startErrorName_ = errorName;
        startErrorMessage_ = errorMessage;
        emitStartFinished();
    }

    void completeStopSuccess()
    {
        stopSucceeds_ = true;
        emitStopFinished();
    }

    void completeStopFailure(const QString& errorName, const QString& errorMessage)
    {
        stopSucceeds_ = false;
        stopErrorName_ = errorName;
        stopErrorMessage_ = errorMessage;
        emitStopFinished();
    }

    void failSnapshot(const QString& errorName, const QString& errorMessage)
    {
        emit snapshotFailed(errorName, errorMessage);
    }

    void setAdapter(const QString& path, const QVariantMap& properties)
    {
        QVariantMap interfaces = objects_.value(path).toMap();
        interfaces.insert(QStringLiteral("org.bluez.Adapter1"), properties);
        objects_.insert(path, interfaces);
        emit interfacesAdded(path, interfaces);
    }

    void replaceAdapter(const QString& path, const QVariantMap& properties)
    {
        QVariantMap interfaces = objects_.value(path).toMap();
        interfaces.insert(QStringLiteral("org.bluez.Adapter1"), properties);
        objects_.insert(path, interfaces);
    }

    void updateAdapter(const QString& path, const QVariantMap& changed, const QStringList& invalidated = {})
    {
        QVariantMap interfaces = objects_.value(path).toMap();
        QVariantMap properties = interfaces.value(QStringLiteral("org.bluez.Adapter1")).toMap();
        for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
            properties.insert(it.key(), it.value());
        }
        for (const QString& key : invalidated) {
            properties.remove(key);
        }
        interfaces.insert(QStringLiteral("org.bluez.Adapter1"), properties);
        objects_.insert(path, interfaces);
        emit propertiesChanged(path, QStringLiteral("org.bluez.Adapter1"), changed, invalidated);
    }

    void removeAdapter(const QString& path)
    {
        QVariantMap interfaces = objects_.value(path).toMap();
        interfaces.remove(QStringLiteral("org.bluez.Adapter1"));
        if (interfaces.isEmpty()) {
            objects_.remove(path);
        } else {
            objects_.insert(path, interfaces);
        }
        emit interfacesRemoved(path, QStringList{QStringLiteral("org.bluez.Adapter1")});
    }

    void setDevice(const QString& path, const QVariantMap& properties)
    {
        QVariantMap interfaces = objects_.value(path).toMap();
        interfaces.insert(QStringLiteral("org.bluez.Device1"), properties);
        objects_.insert(path, interfaces);
        emit interfacesAdded(path, interfaces);
    }

    void updateDevice(const QString& path, const QVariantMap& changed, const QStringList& invalidated = {})
    {
        emit propertiesChanged(path, QStringLiteral("org.bluez.Device1"), changed, invalidated);
    }

    void removeDevice(const QString& path)
    {
        objects_.remove(path);
        emit interfacesRemoved(path, QStringList{QStringLiteral("org.bluez.Device1")});
    }

    int snapshotRequests() const { return snapshotRequests_; }
    int startRequests() const { return startRequests_; }
    int stopRequests() const { return stopRequests_; }
    QString lastStartPath() const { return lastStartPath_; }
    QString lastStopPath() const { return lastStopPath_; }

private:
    void emitStartFinished()
    {
        const QString path = pendingStartPath_.isEmpty() ? lastStartPath_ : pendingStartPath_;
        pendingStartPath_.clear();
        if (!startSucceeds_) {
            emit startDiscoveryFinished(path, false, startErrorName_, startErrorMessage_);
            return;
        }
        emit startDiscoveryFinished(path, true, {}, {});
    }

    void emitStopFinished()
    {
        const QString path = pendingStopPath_.isEmpty() ? lastStopPath_ : pendingStopPath_;
        pendingStopPath_.clear();
        if (!stopSucceeds_) {
            emit stopDiscoveryFinished(path, false, stopErrorName_, stopErrorMessage_);
            return;
        }
        emit stopDiscoveryFinished(path, true, {}, {});
    }

    bool initialized_ = false;
    bool systemBusConnected_ = true;
    bool blueZAvailable_ = true;
    bool startSucceeds_ = true;
    bool stopSucceeds_ = true;
    bool snapshotSucceeds_ = true;
    bool autoCompleteStart_ = true;
    bool autoCompleteStop_ = true;
    bool autoCompleteDeviceOps_ = true;
    bool autoCompleteCancelPairing_ = true;
    bool pairSucceeds_ = true;
    bool cancelPairingSucceeds_ = true;
    bool connectSucceeds_ = true;
    bool agentRegistered_ = false;
    QString pairErrorName_;
    QString pairErrorMessage_;
    QString connectErrorName_;
    QString connectErrorMessage_;
    QString cancelPairingErrorName_;
    QString cancelPairingErrorMessage_;
    QString startErrorName_;
    QString startErrorMessage_;
    QString stopErrorName_;
    QString stopErrorMessage_;
    QString snapshotErrorName_;
    QString snapshotErrorMessage_;
    QVariantMap objects_;
    int snapshotRequests_ = 0;
    int startRequests_ = 0;
    int stopRequests_ = 0;
    int pairRequests_ = 0;
    int cancelPairRequests_ = 0;
    int connectRequests_ = 0;
    int disconnectRequests_ = 0;
    int trustRequests_ = 0;
    int removeRequests_ = 0;
    int registerAgentRequests_ = 0;
    QString lastStartPath_;
    QString lastStopPath_;
    QString lastPairPath_;
    QString lastCancelPairPath_;
    QString lastConnectPath_;
    QString lastRemoveAdapterPath_;
    QString lastRemoveDevicePath_;
    QString lastRegisterAgentPath_;
    QString lastRegisterAgentCapability_;
    QString pendingStartPath_;
    QString pendingStopPath_;
};

} // namespace auralis::test
