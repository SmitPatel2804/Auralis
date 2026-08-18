#pragma once

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

class PipeWireConnection {
public:
    using EventHandler = std::function<void(const PipeWireClientEvent&)>;

    PipeWireConnection();
    ~PipeWireConnection();

    PipeWireConnection(const PipeWireConnection&) = delete;
    PipeWireConnection& operator=(const PipeWireConnection&) = delete;

    bool start(EventHandler handler);
    void stop();
    bool isStarted() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace auralis::audio
