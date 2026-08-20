#pragma once

#include <auralis/audio/PipeWireTypes.h>

#include <QHash>
#include <QVector>

#include <optional>

namespace auralis::audio {

class PipeWireObjectStore {
public:
    bool upsert(const PipeWireObjectSnapshot& snapshot);
    bool remove(quint32 globalId);
    void clear();

    const PipeWireDeviceInfo* device(quint32 globalId) const;
    const PipeWireNodeInfo* node(quint32 globalId) const;
    const PipeWirePortInfo* port(quint32 globalId) const;
    const PipeWireLinkInfo* link(quint32 globalId) const;
    QVector<PipeWireDeviceInfo> devices() const;
    QVector<PipeWireNodeInfo> nodes() const;
    QVector<PipeWirePortInfo> ports() const;
    QVector<PipeWireLinkInfo> links() const;
    QVector<PipeWirePortInfo> portsForNode(quint32 nodeId) const;

    /// Returns a link global id that blocks the requested connection on the input side
    /// (a different producer already feeding the same input port). Output fan-out is allowed.
    std::optional<quint32> findConflictingLinkGlobalId(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort) const;

    /// Returns an active link that already connects the exact endpoints.
    std::optional<quint32> findExactLinkGlobalId(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort) const;

    /// Returns any non-error link using the given node port as output or input.
    std::optional<quint32> findAnyLinkOnPort(quint32 nodeId, quint32 portId, bool asOutput) const;

    int deviceCount() const noexcept;
    int nodeCount() const noexcept;
    int portCount() const noexcept;
    int linkCount() const noexcept;
    int metadataCount() const noexcept;
    int otherCount() const noexcept;

private:
    bool occupiedByOtherKind(quint32 globalId, PipeWireObjectKind kind) const;

    QHash<quint32, PipeWireDeviceInfo> devices_;
    QHash<quint32, PipeWireNodeInfo> nodes_;
    QHash<quint32, PipeWirePortInfo> ports_;
    QHash<quint32, PipeWireLinkInfo> links_;
    QHash<quint32, PipeWireObjectKind> extras_;
    int metadataCount_ = 0;
    int otherCount_ = 0;
};

} // namespace auralis::audio
