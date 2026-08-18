#pragma once

#include <auralis/bluetooth/BluetoothError.h>
#include <auralis/bluetooth/IBluetoothManager.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>

namespace auralis::bluetooth {

class AdapterManager;
class BluetoothDeviceListModel;
class DeviceRegistry;
class DiscoveryManager;
class IBlueZClient;

class BluetoothManager final : public QObject, public IBluetoothManager {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY adapterChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool canStartScan READ canStartScan NOTIFY scanningChanged)
    Q_PROPERTY(bool canStopScan READ canStopScan NOTIFY scanningChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY adapterChanged)
    Q_PROPERTY(bool adapterDiscovering READ adapterDiscovering NOTIFY adapterChanged)
    Q_PROPERTY(QAbstractItemModel* devices READ devices CONSTANT)

public:
    explicit BluetoothManager(QObject* parent = nullptr);
    explicit BluetoothManager(IBlueZClient* client, QObject* parent = nullptr);
    ~BluetoothManager() override;

    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;

    bool available() const noexcept;
    bool adapterPowered() const;
    bool scanning() const noexcept;
    bool canStartScan() const;
    bool canStopScan() const;
    int deviceCount() const;
    QString statusText() const;
    QString errorText() const;
    QString adapterName() const;
    QString adapterAddress() const;
    bool adapterDiscovering() const;
    QAbstractItemModel* devices() const;

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void refresh();

signals:
    void availableChanged();
    void adapterChanged();
    void scanningChanged();
    void deviceCountChanged();
    void statusTextChanged();
    void errorTextChanged();

private:
    void connectClientSignals();
    void handleSnapshot(const QVariantMap& objectsByPath);
    void handleInterfacesAdded(const QString& objectPath, const QVariantMap& interfaces);
    void handleInterfacesRemoved(const QString& objectPath, const QStringList& interfaces);
    void handlePropertiesChanged(
        const QString& objectPath,
        const QString& interfaceName,
        const QVariantMap& changed,
        const QStringList& invalidated);
    void handleBlueZAvailable(bool available);
    void updateStatusText();
    QVariantMap deviceProperties(const QVariantMap& interfaces) const;
    QVariantMap adapterProperties(const QVariantMap& interfaces) const;

    IBlueZClient* client_ = nullptr;
    AdapterManager* adapters_ = nullptr;
    DiscoveryManager* discovery_ = nullptr;
    DeviceRegistry* registry_ = nullptr;
    BluetoothDeviceListModel* model_ = nullptr;
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
    bool available_ = false;
    bool signalsWired_ = false;
    QString statusText_;
    QString errorText_;
};

} // namespace auralis::bluetooth
