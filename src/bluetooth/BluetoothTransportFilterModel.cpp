#include <auralis/bluetooth/BluetoothTransportFilterModel.h>

#include <auralis/bluetooth/BluetoothDeviceListModel.h>

namespace auralis::bluetooth {

BluetoothTransportFilterModel::BluetoothTransportFilterModel(bool lowEnergyOnly, QObject* parent)
    : QSortFilterProxyModel(parent)
    , lowEnergyOnly_(lowEnergyOnly)
{
    setDynamicSortFilter(true);
}

bool BluetoothTransportFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (sourceModel() == nullptr) return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const bool lowEnergy = sourceModel()->data(index, BluetoothDeviceListModel::TransportHintRole).toString()
        == QLatin1String("BLE");
    return lowEnergyOnly_ == lowEnergy;
}

} // namespace auralis::bluetooth
