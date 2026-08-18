#pragma once

#include <auralis/audio/PipeWireTypes.h>

#include <QHash>
#include <QVector>

namespace auralis::audio {

class PipeWireObjectStore {
public:
    bool upsert(const PipeWireObjectSnapshot& snapshot);
    bool remove(quint32 globalId);
    void clear();

    const PipeWireDeviceInfo* device(quint32 globalId) const;
    const PipeWireNodeInfo* node(quint32 globalId) const;
    QVector<PipeWireDeviceInfo> devices() const;
    QVector<PipeWireNodeInfo> nodes() const;

    int deviceCount() const noexcept;
    int nodeCount() const noexcept;
    int portCount() const noexcept;
    int linkCount() const noexcept;
    int metadataCount() const noexcept;
    int otherCount() const noexcept;

private:
    QHash<quint32, PipeWireDeviceInfo> devices_;
    QHash<quint32, PipeWireNodeInfo> nodes_;
    QHash<quint32, PipeWireObjectKind> extras_;
    int portCount_ = 0;
    int linkCount_ = 0;
    int metadataCount_ = 0;
    int otherCount_ = 0;
};

} // namespace auralis::audio
