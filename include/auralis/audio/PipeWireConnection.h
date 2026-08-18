#pragma once

#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/PipeWireTypes.h>

#include <functional>

namespace auralis::audio {

struct PipeWireClientEvent {
    enum class Type {
        StateChanged,
        GlobalAdded,
        GlobalUpdated,
        GlobalRemoved,
        InitialSyncDone
    };

    Type type = Type::StateChanged;
    PipeWireConnectionState state = PipeWireConnectionState::Stopped;
    QString error;
    PipeWireObjectSnapshot snapshot;
    quint32 removedId = 0;
};

class PipeWireConnection final : public IPipeWireLinkBackend {
public:
    using EventHandler = std::function<void(const PipeWireClientEvent&)>;

    PipeWireConnection();
    ~PipeWireConnection() override;

    PipeWireConnection(const PipeWireConnection&) = delete;
    PipeWireConnection& operator=(const PipeWireConnection&) = delete;

    bool start(EventHandler handler);
    void stop();
    bool isStarted() const noexcept;

    std::optional<quint32> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString& routeId,
        const QHash<QString, QString>& extraProps = {}) override;
    bool destroyOwnedLink(quint32 globalId) override;
    bool setNodeVolume(quint32 nodeId, double volume) override;
    bool setNodeMuted(quint32 nodeId, bool muted) override;
    bool volumeSupported(quint32 nodeId) const override;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace auralis::audio
