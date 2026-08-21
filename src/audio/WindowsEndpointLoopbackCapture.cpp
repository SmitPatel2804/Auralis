#include "WindowsEndpointLoopbackCapture.h"

#include <auralis/core/LoggingCategories.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <atomic>
#include <cstring>
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
    const QString text = length > 0 ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed() : QString();
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

} // namespace

struct WindowsEndpointLoopbackCapture::Impl {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    HANDLE sampleReady = nullptr;
    HANDLE stopRequested = nullptr;
    std::thread thread;
    std::atomic_bool isRunning = false;
    std::atomic<quint64> packetCount = 0;
    std::atomic<quint64> byteCount = 0;
    QString endpointDescription;
    WAVEFORMATEX format{};
    DataCallback onData;
    ErrorCallback onError;
    bool comInitialized = false;

    ~Impl() { stop(); }

    void report(HRESULT hr, const QString& context)
    {
        if (onError) onError(QStringLiteral("%1: %2").arg(context, hresultText(hr)));
    }

    void stop()
    {
        const bool hadResources = client != nullptr || thread.joinable() || sampleReady != nullptr;
        if (stopRequested != nullptr) SetEvent(stopRequested);
        if (thread.joinable()) thread.join();
        if (client != nullptr) client->Stop();
        isRunning = false;
        capture.Reset();
        client.Reset();
        endpoint.Reset();
        enumerator.Reset();
        if (sampleReady != nullptr) { CloseHandle(sampleReady); sampleReady = nullptr; }
        if (stopRequested != nullptr) { CloseHandle(stopRequested); stopRequested = nullptr; }
        if (comInitialized) { CoUninitialize(); comInitialized = false; }
        if (hadResources) {
            qCInfo(auralisAudio) << "WindowsEndpointCaptureStopped endpoint=" << endpointDescription
                                 << "packets=" << packetCount.load()
                                 << "bytes=" << byteCount.load();
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
            if (wait != WAIT_OBJECT_0 + 1) continue;

            UINT32 frames = 0;
            HRESULT hr = capture->GetNextPacketSize(&frames);
            while (SUCCEEDED(hr) && frames > 0 && isRunning) {
                BYTE* data = nullptr;
                DWORD flags = 0;
                hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;
                const qsizetype bytes = static_cast<qsizetype>(frames) * format.nBlockAlign;
                QByteArray pcm(bytes, '\0');
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data != nullptr) {
                    std::memcpy(pcm.data(), data, static_cast<std::size_t>(bytes));
                }
                capture->ReleaseBuffer(frames);
                ++packetCount;
                byteCount += static_cast<quint64>(bytes);
                if (onData && !pcm.isEmpty()) onData(std::move(pcm));
                hr = capture->GetNextPacketSize(&frames);
            }
            if (FAILED(hr)) {
                report(hr, QStringLiteral("Windows endpoint loopback capture failed"));
                break;
            }
        }
        isRunning = false;
        if (uninitialize) CoUninitialize();
    }
};

WindowsEndpointLoopbackCapture::WindowsEndpointLoopbackCapture()
    : impl_(std::make_unique<Impl>())
{
}

WindowsEndpointLoopbackCapture::~WindowsEndpointLoopbackCapture() = default;

bool WindowsEndpointLoopbackCapture::start(
    const QByteArray& endpointId,
    const QAudioFormat& audioFormat,
    DataCallback onData,
    ErrorCallback onError,
    QString* errorText)
{
    stop();
    if (errorText != nullptr) errorText->clear();
    if (endpointId.isEmpty() || !audioFormat.isValid()) {
        if (errorText != nullptr) *errorText = QStringLiteral("Invalid Windows audio endpoint or format.");
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
    impl_->onData = std::move(onData);
    impl_->onError = std::move(onError);
    impl_->packetCount = 0;
    impl_->byteCount = 0;
    impl_->endpointDescription = QString::fromUtf8(endpointId);
    impl_->format = waveFormat(audioFormat);
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
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            0,
            0,
            &impl_->format,
            nullptr);
    }
    if (SUCCEEDED(hr)) hr = impl_->client->GetService(IID_PPV_ARGS(&impl_->capture));
    if (SUCCEEDED(hr)) hr = impl_->client->SetEventHandle(impl_->sampleReady);
    if (SUCCEEDED(hr)) hr = impl_->client->Start();
    if (FAILED(hr)) {
        const QString message = QStringLiteral("Unable to capture Auralis Virtual Output: %1").arg(hresultText(hr));
        if (errorText != nullptr) *errorText = message;
        impl_->stop();
        return false;
    }

    impl_->isRunning = true;
    impl_->thread = std::thread([state = impl_.get()] { state->run(); });
    UINT32 bufferFrames = 0;
    impl_->client->GetBufferSize(&bufferFrames);
    const double bufferMilliseconds = audioFormat.sampleRate() > 0
        ? static_cast<double>(bufferFrames) * 1000.0 / audioFormat.sampleRate()
        : -1.0;
    qCInfo(auralisAudio) << "WindowsEndpointCaptureStarted endpoint=" << impl_->endpointDescription
                         << "rate=" << audioFormat.sampleRate()
                         << "channels=" << audioFormat.channelCount()
                         << "sampleFormat=" << static_cast<int>(audioFormat.sampleFormat())
                         << "engineBufferFrames=" << bufferFrames
                         << "engineBufferMs=" << bufferMilliseconds;
    return true;
}

void WindowsEndpointLoopbackCapture::stop() { impl_->stop(); }
bool WindowsEndpointLoopbackCapture::running() const noexcept { return impl_->isRunning; }

} // namespace auralis::audio
