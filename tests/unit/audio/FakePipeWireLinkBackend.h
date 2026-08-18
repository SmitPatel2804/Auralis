#pragma once

#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/PipeWireObjectStore.h>

#include "AudioTestFixtures.h"

#include <QSet>

namespace auralis::test {

class FakePipeWireLinkBackend final : public auralis::audio::IPipeWireLinkBackend {
public:
    explicit FakePipeWireLinkBackend(auralis::audio::PipeWireObjectStore* store)
        : store_(store)
    {
    }

    int createCalls = 0;
    int failOnCreate = -1;
    QString createdLinkState = QStringLiteral("active");
    QSet<quint32> unsupportedVolumeNodes;
    QSet<quint32> failVolumeNodes;
    QVector<quint32> createdIds;
    double lastVolume = 1.0;

    std::optional<quint32> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString&,
        const QHash<QString, QString>& = {}) override
    {
        ++createCalls;
        if (failOnCreate >= 0 && createCalls > failOnCreate) {
            return std::nullopt;
        }
        const quint32 id = nextId_++;
        createdIds.push_back(id);
        if (store_ != nullptr) {
            store_->upsert(makeLink(id, outputNode, outputPort, inputNode, inputPort, createdLinkState));
        }
        return id;
    }

    bool destroyOwnedLink(quint32 globalId) override
    {
        createdIds.removeAll(globalId);
        if (store_ != nullptr) {
            store_->remove(globalId);
        }
        return true;
    }

    bool setNodeVolume(quint32 nodeId, double volume) override
    {
        if (unsupportedVolumeNodes.contains(nodeId) || failVolumeNodes.contains(nodeId)) {
            return false;
        }
        lastVolumeNode = nodeId;
        lastVolume = volume;
        return true;
    }

    bool setNodeMuted(quint32 nodeId, bool muted) override
    {
        if (unsupportedVolumeNodes.contains(nodeId) || failVolumeNodes.contains(nodeId)) {
            return false;
        }
        lastMutedNode = nodeId;
        lastMuted = muted;
        return true;
    }

    bool volumeSupported(quint32 nodeId) const override
    {
        return !unsupportedVolumeNodes.contains(nodeId);
    }

    quint32 lastVolumeNode = 0;
    quint32 lastMutedNode = 0;
    bool lastMuted = false;

private:
    auralis::audio::PipeWireObjectStore* store_ = nullptr;
    quint32 nextId_ = 900;
};

} // namespace auralis::test
