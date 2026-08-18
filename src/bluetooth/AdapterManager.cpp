#include <auralis/bluetooth/AdapterManager.h>

#include <auralis/bluetooth/BlueZConstants.h>
#include <auralis/bluetooth/BlueZPropertyParser.h>

namespace auralis::bluetooth {

AdapterManager::AdapterManager(QObject* parent)
    : QObject(parent)
{
}

bool AdapterManager::isPreferred(const QString& objectPath) const
{
    return objectPath == bluez::kPreferredAdapterPath.toString();
}

void AdapterManager::selectBestAdapter()
{
    QString next;
    if (adapters_.contains(bluez::kPreferredAdapterPath.toString())) {
        next = bluez::kPreferredAdapterPath.toString();
    } else if (!adapters_.isEmpty()) {
        next = adapters_.cbegin().key();
    }

    if (next == selectedPath_) {
        return;
    }
    selectedPath_ = next;
    emit selectedAdapterChanged();
}

void AdapterManager::upsertAdapter(AdapterData adapter)
{
    if (adapter.objectPath.isEmpty()) {
        return;
    }
    adapter.available = true;
    const bool isSelected = adapter.objectPath == selectedPath_;
    adapters_.insert(adapter.objectPath, adapter);
    selectBestAdapter();
    if (isSelected || adapter.objectPath == selectedPath_) {
        emit selectedAdapterUpdated();
    }
}

void AdapterManager::applyPropertyChanges(
    const QString& objectPath,
    const QVariantMap& changed,
    const QStringList& invalidated)
{
    if (!adapters_.contains(objectPath)) {
        return;
    }
    AdapterParseResult parsed = applyAdapterPropertyChanges(adapters_.value(objectPath), changed, invalidated);
    parsed.adapter.objectPath = objectPath;
    parsed.adapter.available = true;
    adapters_.insert(objectPath, parsed.adapter);
    if (objectPath == selectedPath_) {
        emit selectedAdapterUpdated();
    }
}

void AdapterManager::removeAdapter(const QString& objectPath)
{
    if (!adapters_.contains(objectPath)) {
        return;
    }
    adapters_.remove(objectPath);
    if (selectedPath_ == objectPath) {
        selectedPath_.clear();
        selectBestAdapter();
        emit selectedAdapterUpdated();
        return;
    }
    selectBestAdapter();
}

void AdapterManager::reconcile(const QSet<QString>& liveObjectPaths)
{
    const QStringList current = adapters_.keys();
    for (const QString& path : current) {
        if (!liveObjectPaths.contains(path)) {
            removeAdapter(path);
        }
    }
}

void AdapterManager::clear()
{
    if (adapters_.isEmpty()) {
        return;
    }
    adapters_.clear();
    selectedPath_.clear();
    emit selectedAdapterChanged();
    emit selectedAdapterUpdated();
}

bool AdapterManager::hasAdapter() const noexcept
{
    return !selectedPath_.isEmpty();
}

QString AdapterManager::selectedObjectPath() const
{
    return selectedPath_;
}

AdapterData AdapterManager::selected() const
{
    return adapters_.value(selectedPath_);
}

AdapterData AdapterManager::adapterAt(const QString& objectPath) const
{
    return adapters_.value(objectPath);
}

} // namespace auralis::bluetooth
