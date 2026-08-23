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
        InitialSyncDone,
        MetadataChanged
    };

    Type type = Type::StateChanged;
    PipeWireConnectionState state = PipeWireConnectionState::Stopped;
    QString error;
    PipeWireObjectSnapshot snapshot;
    quint32 removedId = 0;
    quint32 metadataSubject = 0;
    QString metadataName;
    QString metadataKey;
    QString metadataType;
    QString metadataValue;
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

    /// Creates the app-lifetime fallback virtual sink. Installed Linux
    /// packages provide the same graph through a persistent PipeWire drop-in.
    bool createVirtualOutput(QString* error = nullptr);
    void destroyVirtualOutput();
    bool ownsVirtualOutput() const noexcept;

    std::optional<LinkCreateResult> createLink(
        quint32 outputNode,
        quint32 outputPort,
        quint32 inputNode,
        quint32 inputPort,
        const QString& routeId,
        const QHash<QString, QString>& extraProps = {}) override;
    bool destroyOwnedLink(quint64 ownershipToken) override;
    bool destroyForeignLink(quint32 globalId) override;
    quint32 ownedLinkGlobalId(quint64 ownershipToken) const override;
    bool setNodeVolume(quint32 nodeId, double volume) override;
    bool setNodeMuted(quint32 nodeId, bool muted) override;
    bool volumeSupported(quint32 nodeId) const override;
    /// Sets PipeWire/WirePlumber current and configured default sink.
    bool setDefaultAudioSink(const QString& nodeName);
    bool setNodeDelaySeconds(quint32 nodeId, double delaySeconds) override;

    using LinkErrorHandler = std::function<void(quint64 ownershipToken, int res, const QString& message)>;
    void setLinkErrorHandler(LinkErrorHandler handler);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    LinkErrorHandler linkErrorHandler_;
};

} // namespace auralis::audio
