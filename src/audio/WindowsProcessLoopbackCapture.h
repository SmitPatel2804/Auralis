#pragma once

#include <QAudioFormat>
#include <QByteArray>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace auralis::audio {

// Captures the audio produced by one Windows process (including its child
// process tree) through the process-loopback WASAPI virtual device.
class WindowsProcessLoopbackCapture final {
public:
    using DataCallback = std::function<void(QByteArray)>;
    using ErrorCallback = std::function<void(QString)>;

    WindowsProcessLoopbackCapture();
    ~WindowsProcessLoopbackCapture();
    WindowsProcessLoopbackCapture(const WindowsProcessLoopbackCapture&) = delete;
    WindowsProcessLoopbackCapture& operator=(const WindowsProcessLoopbackCapture&) = delete;

    bool start(
        std::uint32_t processId,
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
