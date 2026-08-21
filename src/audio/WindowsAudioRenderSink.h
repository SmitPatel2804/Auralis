#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>

namespace auralis::audio {

// Event-driven WASAPI renderer used for non-default Windows endpoints. The
// bounded queue prevents application-side latency from growing indefinitely.
class WindowsAudioRenderSink final {
public:
    using ErrorCallback = std::function<void(QString)>;

    WindowsAudioRenderSink();
    ~WindowsAudioRenderSink();
    WindowsAudioRenderSink(const WindowsAudioRenderSink&) = delete;
    WindowsAudioRenderSink& operator=(const WindowsAudioRenderSink&) = delete;

    bool start(
        const QByteArray& endpointId,
        const QString& endpointDescription,
        const QAudioFormat& format,
        ErrorCallback onError,
        QString* errorText = nullptr);
    void stop();
    bool running() const noexcept;

    qint64 write(const QByteArray& pcm);
    qint64 bufferSize() const noexcept;
    qint64 queuedBytes() const noexcept;
    quint64 droppedBytes() const noexcept;
    void setVolume(double volume) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace auralis::audio
