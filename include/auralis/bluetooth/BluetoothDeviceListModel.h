#pragma once

#include <auralis/bluetooth/DeviceRegistry.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>

namespace auralis::bluetooth {

class BluetoothDeviceListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        InternalIdRole = Qt::UserRole + 1,
        ObjectPathRole,
        AddressRole,
        AddressTypeRole,
        DisplayNameRole,
        NameRole,
        AliasRole,
        RssiRole,
        HasRssiRole,
        PairedRole,
        ConnectedRole,
        TrustedRole,
        BlockedRole,
        ServicesResolvedRole,
        IconRole,
        ClassRole,
        HasClassRole,
        AppearanceRole,
        HasAppearanceRole,
        UuidsRole,
        LastSeenRole,
        TransportHintRole,
        OperationRole,
        LogicalStateRole,
        OperationTextRole,
        LastErrorMessageRole,
        CanPairRole,
        CanCancelPairingRole,
        CanTrustRole,
        CanUntrustRole,
        CanConnectRole,
        CanDisconnectRole,
        CanForgetRole,
        CanReconnectRole,
        CanCancelOperationRole
    };

    explicit BluetoothDeviceListModel(DeviceRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void onDeviceAboutToBeAdded(int index);
    void onDeviceAdded(int index);
    void onDeviceUpdated(int index, const QList<int>& roles);
    void onDeviceAboutToBeRemoved(int index, const QString& objectPath);
    void onDeviceRemoved(int index, const QString& objectPath);

    DeviceRegistry* registry_ = nullptr;
};

} // namespace auralis::bluetooth
