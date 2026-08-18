#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace auralis::bluetooth {

class IBlueZClient : public QObject {
    Q_OBJECT

public:
    explicit IBlueZClient(QObject* parent = nullptr);
    ~IBlueZClient() override = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isSystemBusConnected() const noexcept = 0;
    virtual bool isBlueZAvailable() const noexcept = 0;

    virtual void requestSnapshot() = 0;
    virtual void startDiscovery(const QString& adapterPath) = 0;
    virtual void stopDiscovery(const QString& adapterPath) = 0;

signals:
    void systemBusStateChanged(bool connected);
    void blueZAvailableChanged(bool available);
    void snapshotReceived(const QVariantMap& objectsByPath);
    void snapshotFailed(const QString& errorName, const QString& errorMessage);
    void interfacesAdded(const QString& objectPath, const QVariantMap& interfaces);
    void interfacesRemoved(const QString& objectPath, const QStringList& interfaces);
    void propertiesChanged(
        const QString& objectPath,
        const QString& interfaceName,
        const QVariantMap& changed,
        const QStringList& invalidated);
    void startDiscoveryFinished(
        const QString& adapterPath,
        bool succeeded,
        const QString& errorName,
        const QString& errorMessage);
    void stopDiscoveryFinished(
        const QString& adapterPath,
        bool succeeded,
        const QString& errorName,
        const QString& errorMessage);
};

inline IBlueZClient::IBlueZClient(QObject* parent)
    : QObject(parent)
{
}

} // namespace auralis::bluetooth
