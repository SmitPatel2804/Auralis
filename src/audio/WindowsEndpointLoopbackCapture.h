#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>

namespace auralis::audio {

// Captures the Windows audio-engine mix rendered to one endpoint. This is the
// user-mode bridge for Auralis Virtual Output: no private kernel IPC is needed.
class WindowsEndpointLoopbackCapture final {
public:
    using DataCallback = std::function<void(QByteArray)>;
    using ErrorCallback = std::function<void(QString)>;

    WindowsEndpointLoopbackCapture();
    ~WindowsEndpointLoopbackCapture();
    WindowsEndpointLoopbackCapture(const WindowsEndpointLoopbackCapture&) = delete;
    WindowsEndpointLoopbackCapture& operator=(const WindowsEndpointLoopbackCapture&) = delete;

    bool start(
        const QByteArray& endpointId,
        const QAudioFormat& format,
        DataCallback onData,
        ErrorCallback onError,
        QString* errorText = nullptr);
    void stop();
    bool running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace auralis::audio
