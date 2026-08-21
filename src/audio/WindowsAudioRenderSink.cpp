#include "WindowsAudioRenderSink.h"

#include <auralis/core/LoggingCategories.h>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace auralis::audio {
namespace {

using Microsoft::WRL::ComPtr;

QString hresultText(HRESULT result)
{
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(result),
        0,
        reinterpret_cast<wchar_t*>(&message),
        0,
        nullptr);
    const QString text = length > 0
        ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
        : QString();
    if (message != nullptr) LocalFree(message);
    return text.isEmpty()
        ? QStringLiteral("HRESULT 0x%1").arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'))
        : text;
}

WAVEFORMATEX waveFormat(const QAudioFormat& format)
{
    WAVEFORMATEX wave{};
    wave.wFormatTag = format.sampleFormat() == QAudioFormat::Float ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wave.nChannels = static_cast<WORD>(format.channelCount());
    wave.nSamplesPerSec = static_cast<DWORD>(format.sampleRate());
    wave.wBitsPerSample = static_cast<WORD>(format.bytesPerSample() * 8);
    wave.nBlockAlign = static_cast<WORD>(format.bytesPerFrame());
    wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nBlockAlign;
    return wave;
}

template<typename Sample>
void applySignedGain(Sample* samples, qsizetype count, double gain)
{
    for (qsizetype index = 0; index < count; ++index) {
        const double scaled = static_cast<double>(samples[index]) * gain;
        samples[index] = static_cast<Sample>(std::clamp(
            scaled,
            static_cast<double>(std::numeric_limits<Sample>::min()),
            static_cast<double>(std::numeric_limits<Sample>::max())));
    }
}

} // namespace

struct WindowsAudioRenderSink::Impl {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioRenderClient> render;
    HANDLE sampleReady = nullptr;
    HANDLE stopRequested = nullptr;
    std::thread thread;
    std::atomic_bool isRunning = false;
    std::atomic<qint64> queuedByteCount = 0;
    std::atomic<quint64> renderedByteCount = 0;
    std::atomic<quint64> droppedByteCount = 0;
    std::atomic<double> volume = 1.0;
    std::mutex queueMutex;
    QByteArray queue;
    qsizetype queueOffset = 0;
    qsizetype queueCapacity = 0;
    UINT32 engineBufferFrames = 0;
    WAVEFORMATEX format{};
    QAudioFormat qtFormat;
    QString endpointDescription;
    ErrorCallback onError;
    bool comInitialized = false;

    ~Impl() { stop(); }

    void report(HRESULT hr, const QString& context)
    {
        if (onError) onError(QStringLiteral("%1: %2").arg(context, hresultText(hr)));
    }

    void compactQueueLocked()
    {
        if (queueOffset <= 0) return;
        if (queueOffset >= queue.size()) {
            queue.clear();
            queueOffset = 0;
        } else if (queueOffset >= queue.size() / 2) {
            queue.remove(0, queueOffset);
            queueOffset = 0;
        }
    }

    void applyGain(BYTE* data, qsizetype bytes)
    {
        const double gain = std::clamp(volume.load(), 0.0, 1.0);
        if (gain >= 0.999999 || data == nullptr || bytes <= 0) return;
        if (gain <= 0.000001) {
            std::memset(data, 0, static_cast<std::size_t>(bytes));
            return;
        }
        const qsizetype sampleCount = bytes / std::max<WORD>(1, format.wBitsPerSample / 8);
        switch (qtFormat.sampleFormat()) {
        case QAudioFormat::Float: {
            auto* samples = reinterpret_cast<float*>(data);
            for (qsizetype index = 0; index < sampleCount; ++index) {
                samples[index] = static_cast<float>(samples[index] * gain);
            }
            break;
        }
        case QAudioFormat::Int16:
            applySignedGain(reinterpret_cast<qint16*>(data), sampleCount, gain);
            break;
        case QAudioFormat::Int32:
            applySignedGain(reinterpret_cast<qint32*>(data), sampleCount, gain);
            break;
        case QAudioFormat::UInt8: {
            auto* samples = reinterpret_cast<quint8*>(data);
            for (qsizetype index = 0; index < sampleCount; ++index) {
                const double centered = static_cast<double>(samples[index]) - 128.0;
                samples[index] = static_cast<quint8>(std::clamp(centered * gain + 128.0, 0.0, 255.0));
            }
            break;
        }
        default:
            break;
        }
    }

    void renderAvailableFrames()
    {
        UINT32 paddingFrames = 0;
        HRESULT hr = client->GetCurrentPadding(&paddingFrames);
        if (FAILED(hr)) {
            report(hr, QStringLiteral("Unable to query Windows output padding"));
            isRunning = false;
            return;
        }
        if (paddingFrames >= engineBufferFrames) return;

        const UINT32 availableFrames = engineBufferFrames - paddingFrames;
        BYTE* destination = nullptr;
        hr = render->GetBuffer(availableFrames, &destination);
        if (FAILED(hr)) {
            report(hr, QStringLiteral("Unable to acquire Windows output buffer"));
            isRunning = false;
            return;
        }

        const qsizetype requestedBytes = static_cast<qsizetype>(availableFrames) * format.nBlockAlign;
        qsizetype copiedBytes = 0;
        {
            const std::lock_guard lock(queueMutex);
            const qsizetype availableBytes = std::max<qsizetype>(0, queue.size() - queueOffset);
            copiedBytes = std::min(requestedBytes, availableBytes);
            copiedBytes = copiedBytes / format.nBlockAlign * format.nBlockAlign;
            if (copiedBytes > 0) {
                std::memcpy(destination, queue.constData() + queueOffset, static_cast<std::size_t>(copiedBytes));
                queueOffset += copiedBytes;
                compactQueueLocked();
                queuedByteCount = std::max<qsizetype>(0, queue.size() - queueOffset);
            }
        }

        DWORD flags = 0;
        if (copiedBytes == 0) {
            flags = AUDCLNT_BUFFERFLAGS_SILENT;
        } else {
            if (copiedBytes < requestedBytes) {
                std::memset(
                    destination + copiedBytes,
                    0,
                    static_cast<std::size_t>(requestedBytes - copiedBytes));
            }
            applyGain(destination, requestedBytes);
            renderedByteCount += static_cast<quint64>(copiedBytes);
        }
        hr = render->ReleaseBuffer(availableFrames, flags);
        if (FAILED(hr)) {
            report(hr, QStringLiteral("Unable to release Windows output buffer"));
            isRunning = false;
        }
    }

    void run()
    {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool uninitialize = init == S_OK || init == S_FALSE;
        HANDLE events[]{stopRequested, sampleReady};
        while (isRunning) {
            const DWORD wait = WaitForMultipleObjects(2, events, FALSE, 1000);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_OBJECT_0 + 1) renderAvailableFrames();
        }
        isRunning = false;
        if (uninitialize) CoUninitialize();
    }

    void stop()
    {
        const bool hadResources = client != nullptr || thread.joinable() || sampleReady != nullptr;
        if (stopRequested != nullptr) SetEvent(stopRequested);
        if (thread.joinable()) thread.join();
        if (client != nullptr) client->Stop();
        isRunning = false;
        render.Reset();
        client.Reset();
        endpoint.Reset();
        enumerator.Reset();
        if (sampleReady != nullptr) { CloseHandle(sampleReady); sampleReady = nullptr; }
        if (stopRequested != nullptr) { CloseHandle(stopRequested); stopRequested = nullptr; }
        if (comInitialized) { CoUninitialize(); comInitialized = false; }
        {
            const std::lock_guard lock(queueMutex);
            queue.clear();
            queueOffset = 0;
        }
        queuedByteCount = 0;
        if (hadResources) {
            qCInfo(auralisAudio) << "WindowsAudioOutputStopped endpoint=" << endpointDescription
                                 << "renderedBytes=" << renderedByteCount.load()
                                 << "droppedBytes=" << droppedByteCount.load();
        }
    }
};

WindowsAudioRenderSink::WindowsAudioRenderSink()
    : impl_(std::make_unique<Impl>())
{
}

WindowsAudioRenderSink::~WindowsAudioRenderSink() = default;

bool WindowsAudioRenderSink::start(
    const QByteArray& endpointId,
    const QString& endpointDescription,
    const QAudioFormat& audioFormat,
    ErrorCallback onError,
    QString* errorText)
{
    stop();
    if (errorText != nullptr) errorText->clear();
    if (endpointId.isEmpty() || !audioFormat.isValid() || audioFormat.bytesPerFrame() <= 0) {
        if (errorText != nullptr) *errorText = QStringLiteral("Invalid Windows output endpoint or format.");
        return false;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        if (errorText != nullptr) {
            *errorText = QStringLiteral("Unable to initialize Windows audio COM: %1").arg(hresultText(comResult));
        }
        return false;
    }
    impl_->comInitialized = comResult == S_OK || comResult == S_FALSE;
    impl_->endpointDescription = endpointDescription;
    impl_->qtFormat = audioFormat;
    impl_->format = waveFormat(audioFormat);
    impl_->onError = std::move(onError);
    impl_->renderedByteCount = 0;
    impl_->droppedByteCount = 0;
    impl_->sampleReady = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    impl_->stopRequested = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    HRESULT hr = impl_->sampleReady != nullptr && impl_->stopRequested != nullptr
        ? S_OK
        : HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&impl_->enumerator));
    }
    const QString nativeEndpointId = QString::fromUtf8(endpointId);
    if (SUCCEEDED(hr)) {
        hr = impl_->enumerator->GetDevice(
            reinterpret_cast<LPCWSTR>(nativeEndpointId.utf16()),
            &impl_->endpoint);
    }
    if (SUCCEEDED(hr)) {
        hr = impl_->endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &impl_->client);
    }
    if (SUCCEEDED(hr)) {
        hr = impl_->client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_NOPERSIST,
            0,
            0,
            &impl_->format,
            nullptr);
    }
    if (SUCCEEDED(hr)) hr = impl_->client->GetBufferSize(&impl_->engineBufferFrames);
    if (SUCCEEDED(hr)) hr = impl_->client->GetService(IID_PPV_ARGS(&impl_->render));
    if (SUCCEEDED(hr)) hr = impl_->client->SetEventHandle(impl_->sampleReady);
    if (SUCCEEDED(hr)) hr = impl_->client->Start();
    if (FAILED(hr)) {
        const QString message = QStringLiteral("Unable to start Windows audio output %1: %2")
                                    .arg(endpointDescription, hresultText(hr));
        if (errorText != nullptr) *errorText = message;
        impl_->stop();
        return false;
    }

    // Process loopback delivers 10 ms packets. Keep two packets in user mode
    // so capture and render events may be one packet out of phase without an
    // underrun. If capture gets further ahead, discard the oldest samples so
    // latency cannot grow without bound.
    constexpr qint64 kMaximumQueueUsecs = 20000;
    impl_->queueCapacity = std::max<qsizetype>(
        audioFormat.bytesPerFrame(),
        audioFormat.bytesForDuration(kMaximumQueueUsecs));
    impl_->isRunning = true;
    impl_->thread = std::thread([state = impl_.get()] { state->run(); });
    const double engineBufferMs = audioFormat.sampleRate() > 0
        ? static_cast<double>(impl_->engineBufferFrames) * 1000.0 / audioFormat.sampleRate()
        : -1.0;
    qCInfo(auralisAudio) << "WindowsAudioOutputStarted endpoint=" << endpointDescription
                         << "rate=" << audioFormat.sampleRate()
                         << "channels=" << audioFormat.channelCount()
                         << "sampleFormat=" << static_cast<int>(audioFormat.sampleFormat())
                         << "engineBufferFrames=" << impl_->engineBufferFrames
                         << "engineBufferMs=" << engineBufferMs
                         << "queueCapacityMs="
                         << (audioFormat.durationForBytes(impl_->queueCapacity) / 1000.0);
    return true;
}

void WindowsAudioRenderSink::stop() { impl_->stop(); }
bool WindowsAudioRenderSink::running() const noexcept { return impl_->isRunning; }

qint64 WindowsAudioRenderSink::write(const QByteArray& pcm)
{
    if (!impl_->isRunning || pcm.isEmpty()) return 0;
    const qsizetype frameBytes = std::max<WORD>(1, impl_->format.nBlockAlign);
    qsizetype inputBytes = pcm.size() / frameBytes * frameBytes;
    if (inputBytes <= 0) return 0;

    const std::lock_guard lock(impl_->queueMutex);
    impl_->compactQueueLocked();
    if (inputBytes > impl_->queueCapacity) {
        const qsizetype discarded = inputBytes - impl_->queueCapacity;
        impl_->droppedByteCount += static_cast<quint64>(discarded);
        inputBytes = impl_->queueCapacity;
    }
    const qsizetype existingBytes = impl_->queue.size() - impl_->queueOffset;
    const qsizetype overflow = std::max<qsizetype>(0, existingBytes + inputBytes - impl_->queueCapacity);
    if (overflow > 0) {
        const qsizetype alignedOverflow = (overflow + frameBytes - 1) / frameBytes * frameBytes;
        const qsizetype removed = std::min(alignedOverflow, existingBytes);
        impl_->queueOffset += removed;
        impl_->droppedByteCount += static_cast<quint64>(removed);
        impl_->compactQueueLocked();
    }
    impl_->queue.append(pcm.constData() + (pcm.size() - inputBytes), inputBytes);
    impl_->queuedByteCount = impl_->queue.size() - impl_->queueOffset;
    return inputBytes;
}

qint64 WindowsAudioRenderSink::bufferSize() const noexcept { return impl_->queueCapacity; }
qint64 WindowsAudioRenderSink::queuedBytes() const noexcept { return impl_->queuedByteCount; }
quint64 WindowsAudioRenderSink::droppedBytes() const noexcept { return impl_->droppedByteCount; }
void WindowsAudioRenderSink::setVolume(double value) noexcept { impl_->volume = std::clamp(value, 0.0, 1.0); }

} // namespace auralis::audio
