#include "WindowsProcessLoopbackCapture.h"

#include <auralis/core/LoggingCategories.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <utility>

namespace auralis::audio {
namespace {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;

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

class ActivationHandler final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
    ActivationHandler() : completed_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    ~ActivationHandler() override { if (completed_ != nullptr) CloseHandle(completed_); }

    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override
    {
        HRESULT activationResult = E_UNEXPECTED;
        ComPtr<IUnknown> unknown;
        result_ = operation->GetActivateResult(&activationResult, &unknown);
        if (SUCCEEDED(result_)) result_ = activationResult;
        if (SUCCEEDED(result_)) result_ = unknown.As(&client_);
        SetEvent(completed_);
        return S_OK;
    }

    HRESULT wait(DWORD timeoutMs)
    {
        return WaitForSingleObject(completed_, timeoutMs) == WAIT_OBJECT_0
            ? result_
            : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }

    ComPtr<IAudioClient> client() const { return client_; }

private:
    HANDLE completed_ = nullptr;
    HRESULT result_ = E_UNEXPECTED;
    ComPtr<IAudioClient> client_;
};

} // namespace

struct WindowsProcessLoopbackCapture::Impl {
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    ComPtr<IActivateAudioInterfaceAsyncOperation> activation;
    HANDLE sampleReady = nullptr;
    HANDLE stopRequested = nullptr;
    std::thread thread;
    std::atomic_bool isRunning = false;
    std::atomic<quint64> packetCount = 0;
    std::atomic<quint64> byteCount = 0;
    std::uint32_t processId = 0;
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
        activation.Reset();
        if (sampleReady != nullptr) { CloseHandle(sampleReady); sampleReady = nullptr; }
        if (stopRequested != nullptr) { CloseHandle(stopRequested); stopRequested = nullptr; }
        if (comInitialized) { CoUninitialize(); comInitialized = false; }
        if (hadResources) {
            qCInfo(auralisAudio) << "WindowsProcessCaptureStopped pid=" << processId
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
                report(hr, QStringLiteral("Windows process audio capture failed"));
                break;
            }
        }
        isRunning = false;
        if (uninitialize) CoUninitialize();
    }
};

WindowsProcessLoopbackCapture::WindowsProcessLoopbackCapture()
    : impl_(std::make_unique<Impl>())
{
}

WindowsProcessLoopbackCapture::~WindowsProcessLoopbackCapture() = default;

bool WindowsProcessLoopbackCapture::start(
    std::uint32_t processId,
    const QAudioFormat& audioFormat,
    DataCallback onData,
    ErrorCallback onError,
    QString* errorText)
{
    stop();
    if (errorText != nullptr) errorText->clear();
    if (processId == 0 || !audioFormat.isValid()) {
        if (errorText != nullptr) *errorText = QStringLiteral("Invalid process or audio format.");
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
    impl_->processId = processId;
    impl_->packetCount = 0;
    impl_->byteCount = 0;
    impl_->sampleReady = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    impl_->stopRequested = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl_->sampleReady == nullptr || impl_->stopRequested == nullptr) {
        if (errorText != nullptr) *errorText = QStringLiteral("Unable to create Windows audio events.");
        impl_->stop();
        return false;
    }

    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = processId;
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT variant{};
    variant.vt = VT_BLOB;
    variant.blob.cbSize = sizeof(params);
    variant.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

    const ComPtr<ActivationHandler> handler = Make<ActivationHandler>();
    HRESULT hr = handler != nullptr ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = ActivateAudioInterfaceAsync(
            VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
            __uuidof(IAudioClient),
            &variant,
            handler.Get(),
            &impl_->activation);
    }
    if (SUCCEEDED(hr)) hr = handler->wait(7000);
    if (SUCCEEDED(hr)) impl_->client = handler->client();
    impl_->format = waveFormat(audioFormat);
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
        const QString message = QStringLiteral("Unable to start per-application audio capture: %1").arg(hresultText(hr));
        if (errorText != nullptr) *errorText = message;
        impl_->stop();
        return false;
    }

    impl_->isRunning = true;
    impl_->thread = std::thread([state = impl_.get()] { state->run(); });
    UINT32 bufferFrames = 0;
    const HRESULT bufferResult = impl_->client->GetBufferSize(&bufferFrames);
    const double bufferMilliseconds = SUCCEEDED(bufferResult) && audioFormat.sampleRate() > 0
        ? static_cast<double>(bufferFrames) * 1000.0 / audioFormat.sampleRate()
        : -1.0;
    qCInfo(auralisAudio) << "WindowsProcessCaptureStarted pid=" << processId
                         << "rate=" << audioFormat.sampleRate()
                         << "channels=" << audioFormat.channelCount()
                         << "sampleFormat=" << static_cast<int>(audioFormat.sampleFormat())
                         << "engineBufferFrames=" << bufferFrames
                         << "engineBufferMs=" << bufferMilliseconds;
    return true;
}

void WindowsProcessLoopbackCapture::stop() { impl_->stop(); }
bool WindowsProcessLoopbackCapture::running() const noexcept { return impl_->isRunning; }

} // namespace auralis::audio
