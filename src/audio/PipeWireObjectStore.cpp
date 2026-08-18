#include <auralis/audio/PipeWireObjectStore.h>

namespace auralis::audio {

bool PipeWireObjectStore::occupiedByOtherKind(quint32 globalId, PipeWireObjectKind kind) const
{
    if (devices_.contains(globalId) && kind != PipeWireObjectKind::Device) {
        return true;
    }
    if (nodes_.contains(globalId) && kind != PipeWireObjectKind::Node) {
        return true;
    }
    const auto extra = extras_.constFind(globalId);
    return extra != extras_.cend() && extra.value() != kind;
}

bool PipeWireObjectStore::upsert(const PipeWireObjectSnapshot& snapshot)
{
    if (occupiedByOtherKind(snapshot.globalId, snapshot.kind)) {
        remove(snapshot.globalId);
    }

    switch (snapshot.kind) {
    case PipeWireObjectKind::Device: {
        PipeWireDeviceInfo current = devices_.value(snapshot.globalId);
        mergeDeviceInfo(current, snapshot);
        devices_.insert(snapshot.globalId, current);
        extras_.remove(snapshot.globalId);
        return true;
    }
    case PipeWireObjectKind::Node: {
        PipeWireNodeInfo current = nodes_.value(snapshot.globalId);
        mergeNodeInfo(current, snapshot);
        nodes_.insert(snapshot.globalId, current);
        extras_.remove(snapshot.globalId);
        return true;
    }
    case PipeWireObjectKind::Port:
        if (!extras_.contains(snapshot.globalId)) {
            extras_.insert(snapshot.globalId, snapshot.kind);
            ++portCount_;
        }
        return true;
    case PipeWireObjectKind::Link:
        if (!extras_.contains(snapshot.globalId)) {
            extras_.insert(snapshot.globalId, snapshot.kind);
            ++linkCount_;
        }
        return true;
    case PipeWireObjectKind::Metadata:
        if (!extras_.contains(snapshot.globalId)) {
            extras_.insert(snapshot.globalId, snapshot.kind);
            ++metadataCount_;
        }
        return true;
    case PipeWireObjectKind::Other:
        if (!extras_.contains(snapshot.globalId)) {
            extras_.insert(snapshot.globalId, snapshot.kind);
            ++otherCount_;
        }
        return true;
    case PipeWireObjectKind::Unknown:
        return false;
    }
    return false;
}

bool PipeWireObjectStore::remove(quint32 globalId)
{
    if (devices_.remove(globalId) > 0) {
        return true;
    }
    if (nodes_.remove(globalId) > 0) {
        return true;
    }
    const auto it = extras_.constFind(globalId);
    if (it == extras_.cend()) {
        return false;
    }
    switch (it.value()) {
    case PipeWireObjectKind::Port:
        --portCount_;
        break;
    case PipeWireObjectKind::Link:
        --linkCount_;
        break;
    case PipeWireObjectKind::Metadata:
        --metadataCount_;
        break;
    case PipeWireObjectKind::Other:
        --otherCount_;
        break;
    default:
        break;
    }
    extras_.erase(it);
    return true;
}

void PipeWireObjectStore::clear()
{
    devices_.clear();
    nodes_.clear();
    extras_.clear();
    portCount_ = 0;
    linkCount_ = 0;
    metadataCount_ = 0;
    otherCount_ = 0;
}

const PipeWireDeviceInfo* PipeWireObjectStore::device(quint32 globalId) const
{
    const auto it = devices_.constFind(globalId);
    if (it == devices_.cend()) {
        return nullptr;
    }
    return &it.value();
}

const PipeWireNodeInfo* PipeWireObjectStore::node(quint32 globalId) const
{
    const auto it = nodes_.constFind(globalId);
    if (it == nodes_.cend()) {
        return nullptr;
    }
    return &it.value();
}

QVector<PipeWireDeviceInfo> PipeWireObjectStore::devices() const
{
    QVector<PipeWireDeviceInfo> result;
    result.reserve(static_cast<qsizetype>(devices_.size()));
    for (const PipeWireDeviceInfo& device : devices_) {
        result.push_back(device);
    }
    return result;
}

QVector<PipeWireNodeInfo> PipeWireObjectStore::nodes() const
{
    QVector<PipeWireNodeInfo> result;
    result.reserve(static_cast<qsizetype>(nodes_.size()));
    for (const PipeWireNodeInfo& node : nodes_) {
        result.push_back(node);
    }
    return result;
}

int PipeWireObjectStore::deviceCount() const noexcept
{
    return static_cast<int>(devices_.size());
}

int PipeWireObjectStore::nodeCount() const noexcept
{
    return static_cast<int>(nodes_.size());
}

int PipeWireObjectStore::portCount() const noexcept
{
    return portCount_;
}

int PipeWireObjectStore::linkCount() const noexcept
{
    return linkCount_;
}

int PipeWireObjectStore::metadataCount() const noexcept
{
    return metadataCount_;
}

int PipeWireObjectStore::otherCount() const noexcept
{
    return otherCount_;
}

} // namespace auralis::audio
