#include <auralis/bluetooth/DeviceRegistry.h>

#include <auralis/bluetooth/BlueZPropertyParser.h>

#include <QDateTime>

#include <functional>

namespace auralis::bluetooth {

DeviceRegistry::DeviceRegistry(QObject* parent)
    : QObject(parent)
{
}

void DeviceRegistry::touchLastSeen(BluetoothDeviceData& device) const
{
    device.lastSeen = QDateTime::currentDateTimeUtc();
}

bool DeviceRegistry::upsertDevice(BluetoothDeviceData device)
{
    if (device.objectPath.isEmpty()) {
        return false;
    }

    touchLastSeen(device);
    const int existing = indexOf(device.objectPath);
    if (existing >= 0) {
        const BluetoothDeviceData previous = byPath_.value(device.objectPath);
        device.operation = previous.operation;
        device.lastError = previous.lastError;
        device.lastErrorName = previous.lastErrorName;
        device.lastErrorMessage = previous.lastErrorMessage;
        device.lastErrorTimestamp = previous.lastErrorTimestamp;
        device.reconnectAttempt = previous.reconnectAttempt;
        device.userDisconnectRequested = previous.userDisconnectRequested;
        device.autoReconnectEnabled = previous.autoReconnectEnabled;
        byPath_.insert(device.objectPath, device);
        if (!device.addressIndexKey().isEmpty()) {
            byAddressKey_.insert(device.addressIndexKey(), device.objectPath);
        }
        emit deviceUpdated(existing, {});
        return false;
    }

    const int index = static_cast<int>(order_.size());
    emit deviceAboutToBeAdded(index);
    order_.push_back(device.objectPath);
    byPath_.insert(device.objectPath, device);
    if (!device.addressIndexKey().isEmpty()) {
        byAddressKey_.insert(device.addressIndexKey(), device.objectPath);
    }
    emit deviceAdded(index);
    emit countChanged();
    return true;
}

bool DeviceRegistry::applyPropertyChanges(
    const QString& objectPath,
    const QVariantMap& changed,
    const QStringList& invalidated)
{
    const int index = indexOf(objectPath);
    if (index < 0) {
        return false;
    }

    BluetoothDeviceData current = byPath_.value(objectPath);
    const QString previousKey = current.addressIndexKey();
    DeviceParseResult parsed = applyDevicePropertyChanges(current, changed, invalidated);
    logParseWarnings(objectPath, QStringLiteral("org.bluez.Device1"), parsed.warnings);
    for (const ParseWarning& warning : parsed.warnings) {
        emit parseWarning(objectPath, warning.property, warning.message);
    }
    touchLastSeen(parsed.device);
    parsed.device.operation = current.operation;
    parsed.device.lastError = current.lastError;
    parsed.device.lastErrorName = current.lastErrorName;
    parsed.device.lastErrorMessage = current.lastErrorMessage;
    parsed.device.lastErrorTimestamp = current.lastErrorTimestamp;
    parsed.device.reconnectAttempt = current.reconnectAttempt;
    parsed.device.userDisconnectRequested = current.userDisconnectRequested;
    parsed.device.autoReconnectEnabled = current.autoReconnectEnabled;
    byPath_.insert(objectPath, parsed.device);

    if (previousKey != parsed.device.addressIndexKey()) {
        if (!previousKey.isEmpty()) {
            byAddressKey_.remove(previousKey);
        }
        if (!parsed.device.addressIndexKey().isEmpty()) {
            byAddressKey_.insert(parsed.device.addressIndexKey(), objectPath);
        }
    }

    emit deviceUpdated(index, {});
    return true;
}

void DeviceRegistry::removeDevice(const QString& objectPath)
{
    const int index = indexOf(objectPath);
    if (index < 0) {
        return;
    }

    emit deviceAboutToBeRemoved(index, objectPath);
    const BluetoothDeviceData removed = byPath_.take(objectPath);
    if (!removed.addressIndexKey().isEmpty()) {
        byAddressKey_.remove(removed.addressIndexKey());
    }
    order_.removeAt(index);
    emit deviceRemoved(index, objectPath);
    emit countChanged();
}

void DeviceRegistry::reconcile(const QSet<QString>& liveObjectPaths)
{
    const QStringList current = order_;
    for (const QString& path : current) {
        if (!liveObjectPaths.contains(path)) {
            removeDevice(path);
        }
    }
}

void DeviceRegistry::clear()
{
    if (order_.isEmpty()) {
        return;
    }

    for (int i = static_cast<int>(order_.size()) - 1; i >= 0; --i) {
        removeDevice(order_.at(i));
    }
}

int DeviceRegistry::count() const noexcept
{
    return static_cast<int>(order_.size());
}

int DeviceRegistry::indexOf(const QString& objectPath) const
{
    return static_cast<int>(order_.indexOf(objectPath));
}

const BluetoothDeviceData* DeviceRegistry::findByObjectPath(const QString& objectPath) const
{
    const auto it = byPath_.constFind(objectPath);
    if (it == byPath_.cend()) {
        return nullptr;
    }
    return &it.value();
}

const BluetoothDeviceData* DeviceRegistry::findByAddress(
    const QString& adapterPath,
    const QString& address,
    const QString& addressType) const
{
    const QString key = addressIndexKey(adapterPath, address, addressType);
    if (key.isEmpty()) {
        return nullptr;
    }
    const QString path = byAddressKey_.value(key);
    if (path.isEmpty()) {
        return nullptr;
    }
    return findByObjectPath(path);
}

BluetoothDeviceData DeviceRegistry::at(int index) const
{
    if (index < 0 || index >= order_.size()) {
        return {};
    }
    return byPath_.value(order_.at(index));
}

QVector<BluetoothDeviceData> DeviceRegistry::devices() const
{
    QVector<BluetoothDeviceData> result;
    result.reserve(order_.size());
    for (const QString& path : order_) {
        result.push_back(byPath_.value(path));
    }
    return result;
}

bool DeviceRegistry::mutateDevice(const QString& objectPath, const std::function<void(BluetoothDeviceData&)>& mutator)
{
    const int index = indexOf(objectPath);
    if (index < 0) {
        return false;
    }
    BluetoothDeviceData device = byPath_.value(objectPath);
    mutator(device);
    byPath_.insert(objectPath, device);
    emit deviceUpdated(index, {});
    return true;
}

bool DeviceRegistry::setOperation(const QString& objectPath, DeviceOperation operation)
{
    return mutateDevice(objectPath, [operation](BluetoothDeviceData& device) { device.operation = operation; });
}

bool DeviceRegistry::clearOperation(const QString& objectPath)
{
    return setOperation(objectPath, DeviceOperation::Idle);
}

bool DeviceRegistry::setLastError(
    const QString& objectPath,
    BluetoothError error,
    const QString& errorName,
    const QString& message)
{
    return mutateDevice(objectPath, [&](BluetoothDeviceData& device) {
        device.lastError = error;
        device.lastErrorName = errorName;
        device.lastErrorMessage = message;
        device.lastErrorTimestamp = QDateTime::currentDateTimeUtc();
    });
}

bool DeviceRegistry::clearLastError(const QString& objectPath)
{
    return mutateDevice(objectPath, [](BluetoothDeviceData& device) {
        device.lastError = BluetoothError::None;
        device.lastErrorName.clear();
        device.lastErrorMessage.clear();
        device.lastErrorTimestamp = {};
    });
}

bool DeviceRegistry::setUserDisconnectRequested(const QString& objectPath, bool requested)
{
    return mutateDevice(objectPath, [requested](BluetoothDeviceData& device) {
        device.userDisconnectRequested = requested;
    });
}

bool DeviceRegistry::setAutoReconnectEnabled(const QString& objectPath, bool enabled)
{
    return mutateDevice(objectPath, [enabled](BluetoothDeviceData& device) {
        device.autoReconnectEnabled = enabled;
    });
}

bool DeviceRegistry::setReconnectAttempt(const QString& objectPath, int attempt)
{
    return mutateDevice(objectPath, [attempt](BluetoothDeviceData& device) { device.reconnectAttempt = attempt; });
}

} // namespace auralis::bluetooth
