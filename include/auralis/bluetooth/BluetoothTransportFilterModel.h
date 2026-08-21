#pragma once

#include <QSortFilterProxyModel>

namespace auralis::bluetooth {

class BluetoothTransportFilterModel final : public QSortFilterProxyModel {
public:
    explicit BluetoothTransportFilterModel(bool lowEnergyOnly, QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool lowEnergyOnly_ = false;
};

} // namespace auralis::bluetooth
