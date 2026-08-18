#pragma once

#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/PipeWireObjectStore.h>

#include "AudioTestFixtures.h"

#include <QHash>
#include <QSet>
#include <QVector>

namespace auralis::test {

class FakePipeWireLinkBackend final : public auralis::audio::IPipeWireLinkBackend {
public:
    struct Owned {
        quint64 token = 0;
        quint32 globalId = 0;
        quint32 outputNode = 0;
        quint32 outputPort = 0;
        quint32 inputNode = 0;
        quint32 inputPort = 0;
        QString routeId;
    };

    explicit FakePipeWireLinkBackend(auralis::audio::PipeWireObjectStore* store)
        : store_(store)
    {
    }

    int createCalls = 0;
    int failOnCreate = -1;
    bool delayBind = false;
    QString createdLinkState = QStringLiteral("active");
    QSet<quint32> unsupportedVolumeNodes;
    QSet<quint32> failVolumeNodes;
    QVector<quint32> createdIds;
    QVector<quint64> createdTokens;
    QVector<quint64> destroyedTokens;
    QHash<quint64, Owned> owned;
    double lastVolume = 1.0;
    quint32 lastVolumeNode = 0;
    quint32 lastMutedNode = 0;
    bool lastMuted = false;

    std::optional<auralis::audio::LinkCreateResult> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString& routeId,
        const QHash<QString, QString>& = {}) override
    {
        ++createCalls;
        if (failOnCreate >= 0 && createCalls > failOnCreate) {
            return std::nullopt;
        }
        Owned created;
        created.token = nextToken_++;
        created.outputNode = outputNode;
        created.outputPort = outputPort;
        created.inputNode = inputNode;
        created.inputPort = inputPort;
        created.routeId = routeId;
        if (!delayBind) {
            created.globalId = nextId_++;
            createdIds.push_back(created.globalId);
            upsertStore(created);
        }
        owned.insert(created.token, created);
        createdTokens.push_back(created.token);
        return auralis::audio::LinkCreateResult{created.token, created.globalId};
    }

    bool destroyOwnedLink(quint64 ownershipToken) override
    {
        if (ownershipToken == 0) {
            return true;
        }
        const auto it = owned.find(ownershipToken);
        if (it == owned.end()) {
            return true;
        }
        destroyedTokens.push_back(ownershipToken);
        if (it->globalId != 0) {
            createdIds.removeAll(it->globalId);
            if (store_ != nullptr) {
                store_->remove(it->globalId);
            }
        }
        createdTokens.removeAll(ownershipToken);
        owned.erase(it);
        return true;
    }

    quint32 ownedLinkGlobalId(quint64 ownershipToken) const override
    {
        return owned.value(ownershipToken).globalId;
    }

    void completeBind(quint64 ownershipToken, quint32 globalId)
    {
        auto it = owned.find(ownershipToken);
        if (it == owned.end()) {
            return;
        }
        it->globalId = globalId;
        createdIds.push_back(globalId);
        upsertStore(*it);
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

private:
    void upsertStore(const Owned& created)
    {
        if (store_ == nullptr || created.globalId == 0) {
            return;
        }
        auralis::audio::PipeWireObjectSnapshot snapshot = makeLink(
            created.globalId,
            created.outputNode,
            created.outputPort,
            created.inputNode,
            created.inputPort,
            createdLinkState);
        QHash<QString, QString> values = snapshot.properties.toHash();
        if (!created.routeId.isEmpty()) {
            values.insert(QStringLiteral("auralis.route.id"), created.routeId);
        }
        snapshot.properties = auralis::audio::PipeWireProperties::fromHash(std::move(values));
        store_->upsert(snapshot);
    }

    auralis::audio::PipeWireObjectStore* store_ = nullptr;
    quint32 nextId_ = 900;
    quint64 nextToken_ = 1;
};

} // namespace auralis::test
