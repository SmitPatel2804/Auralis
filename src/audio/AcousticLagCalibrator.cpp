#include <auralis/audio/AcousticLagCalibrator.h>

#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/DestinationSync.h>
#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/PipeWireVirtualOutput.h>
#include <auralis/core/LoggingCategories.h>

#include <QDateTime>
#include <QMutex>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <mutex>

#if defined(Q_OS_LINUX)
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#endif

namespace auralis::audio {
namespace {

bool looksLikeBluetoothCapture(const QString& text)
{
    const QString lower = text.toLower();
    return lower.contains(QLatin1String("bluez")) || lower.contains(QLatin1String("headset"))
        || lower.contains(QLatin1String("handsfree")) || lower.contains(QLatin1String("a2dp"))
        || lower.contains(QLatin1String("hfp"));
}

#if defined(Q_OS_LINUX)
QString findLaptopMicNodeName(const PipeWireObjectStore* store)
{
    if (store == nullptr) {
        return {};
    }
    QString alsa;
    QString any;
    for (const PipeWireNodeInfo& node : store->nodes()) {
        if (node.mediaClass != QLatin1String("Audio/Source")) {
            continue;
        }
        const QString blob =
            (node.name + QLatin1Char(' ') + node.description + QLatin1Char(' ')
             + node.properties.value(QStringLiteral("device.api")))
                .toLower();
        if (looksLikeBluetoothCapture(blob) || blob.contains(QLatin1String("auralis"))
            || blob.contains(QLatin1String("monitor"))) {
            continue;
        }
        if (node.properties.boolValue(QStringLiteral("device.default")).value_or(false)) {
            return node.name;
        }
        if (alsa.isEmpty() && node.properties.value(QStringLiteral("device.api")).toLower() == QLatin1String("alsa")) {
            alsa = node.name;
        }
        if (any.isEmpty()) {
            any = node.name;
        }
    }
    return !alsa.isEmpty() ? alsa : any;
}

bool virtualOutputSinkPresent(const PipeWireObjectStore* store)
{
    if (store == nullptr) {
        return false;
    }
    for (const PipeWireNodeInfo& node : store->nodes()) {
        if (isAuralisPipeWireVirtualSink(node)) {
            return true;
        }
    }
    return false;
}
#endif

} // namespace

#if defined(Q_OS_LINUX)
struct PipeWireLagCapture {
    struct Stream {
        PipeWireLagCapture* owner = nullptr;
        pw_stream* stream = nullptr;
        spa_hook listener{};
        bool reference = false;
        int channels = 1;
    };

    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    Stream reference;
    Stream mic;
    QMutex mutex;
    QVector<float> referenceSamples;
    QVector<float> micSamples;
    int rate = 48000;

    static void onProcess(void* data)
    {
        auto* stream = static_cast<Stream*>(data);
        if (stream == nullptr || stream->owner == nullptr || stream->stream == nullptr) {
            return;
        }
        pw_buffer* buffer = pw_stream_dequeue_buffer(stream->stream);
        if (buffer == nullptr || buffer->buffer == nullptr || buffer->buffer->n_datas == 0) {
            return;
        }
        spa_data& data0 = buffer->buffer->datas[0];
        if (data0.data == nullptr || data0.chunk == nullptr || data0.chunk->size == 0) {
            pw_stream_queue_buffer(stream->stream, buffer);
            return;
        }
        const int channels = std::max(1, stream->channels);
        const int frames = static_cast<int>(data0.chunk->size / (sizeof(float) * static_cast<unsigned>(channels)));
        const auto* samples = static_cast<const float*>(data0.data);
        QVector<float>* dest =
            stream->reference ? &stream->owner->referenceSamples : &stream->owner->micSamples;
        const qsizetype keep = static_cast<qsizetype>(stream->owner->rate) * 2;
        {
            QMutexLocker locker(&stream->owner->mutex);
            dest->reserve(keep);
            for (int frame = 0; frame < frames; ++frame) {
                float mix = 0.0f;
                for (int channel = 0; channel < channels; ++channel) {
                    mix += samples[frame * channels + channel];
                }
                dest->push_back(mix / static_cast<float>(channels));
            }
            if (dest->size() > keep) {
                dest->erase(dest->begin(), dest->begin() + (dest->size() - keep));
            }
        }
        pw_stream_queue_buffer(stream->stream, buffer);
    }
};

namespace {

const pw_stream_events kLagStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = nullptr,
    .state_changed = nullptr,
    .control_info = nullptr,
    .io_changed = nullptr,
    .param_changed = nullptr,
    .add_buffer = nullptr,
    .remove_buffer = nullptr,
    .process = &PipeWireLagCapture::onProcess,
    .drained = nullptr,
    .command = nullptr,
    .trigger_done = nullptr,
};

pw_stream* connectCaptureStream(
    pw_core* core,
    PipeWireLagCapture::Stream* stream,
    const char* nodeName,
    const char* target,
    bool captureSink,
    int rate)
{
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE,
        "Audio",
        PW_KEY_MEDIA_CATEGORY,
        "Capture",
        PW_KEY_MEDIA_ROLE,
        "DSP",
        PW_KEY_NODE_NAME,
        nodeName,
        PW_KEY_NODE_DESCRIPTION,
        "Auralis lag capture",
        PW_KEY_NODE_VIRTUAL,
        "true",
        PW_KEY_STREAM_DONT_REMIX,
        "false",
        nullptr);
    if (captureSink) {
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }
    if (target != nullptr && target[0] != '\0') {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, target);
        pw_properties_set(props, PW_KEY_NODE_DONT_RECONNECT, "true");
        pw_properties_set(props, "node.dont-fallback", "true");
    }
    stream->stream = pw_stream_new(core, nodeName, props);
    if (stream->stream == nullptr) {
        return nullptr;
    }
    pw_stream_add_listener(stream->stream, &stream->listener, &kLagStreamEvents, stream);

    uint8_t podBuffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.channels = 1;
    info.rate = static_cast<uint32_t>(rate);
    info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    stream->channels = 1;
    const int rc = pw_stream_connect(
        stream->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params,
        1);
    if (rc < 0) {
        pw_stream_destroy(stream->stream);
        stream->stream = nullptr;
        return nullptr;
    }
    return stream->stream;
}

} // namespace
#else
struct PipeWireLagCapture {};
#endif

AcousticLagCalibrator::AcousticLagCalibrator(
    AudioRouter* router,
    PipeWireObjectStore* store,
    AudioEndpointRegistry* endpoints,
    IPipeWireLinkBackend* backend,
    QObject* parent)
    : QObject(parent)
    , router_(router)
    , store_(store)
    , endpoints_(endpoints)
    , backend_(backend)
{
    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &AcousticLagCalibrator::onSampleTimer);
    sampleTimer_ = timer;
}

AcousticLagCalibrator::~AcousticLagCalibrator()
{
    stop();
}

QString AcousticLagCalibrator::statusText() const
{
    return status_;
}

void AcousticLagCalibrator::maybeStart(const QString& sessionId, const QStringList& endpointIds)
{
    if (running_ || router_ == nullptr || sessionId.isEmpty() || endpointIds.size() < 2) {
        return;
    }
    if (calibratedSessions_.contains(sessionId) || ignoreSession_ == sessionId) {
        return;
    }
    endpointIds_ = endpointIds;
    sessionId_ = sessionId;
    if (!startCapture()) {
        qCWarning(auralisAudio)
            << "AcousticLagCalibrationSkipped session=" << sessionId
            << "reason=need-laptop-mic-and-virtual-output-monitor";
        status_ = QStringLiteral("Could not measure lag (need a laptop mic near both headsets)");
        emit statusChanged();
        ignoreSession_ = sessionId;
        return;
    }
    running_ = true;
    phaseIndex_ = 0;
    startedMs_ = QDateTime::currentMSecsSinceEpoch();
    phaseStartedMs_ = startedMs_;
    applySolo(0);
    status_ = QStringLiteral("Measuring headset delay (10s). Keep both headsets near the laptop mic.");
    emit statusChanged();
    qCInfo(auralisAudio) << "AcousticLagCalibrationStarted session=" << sessionId
                         << "devices=" << endpointIds.size();
    if (auto* timer = qobject_cast<QTimer*>(sampleTimer_)) {
        timer->start();
    }
}

void AcousticLagCalibrator::stop()
{
    if (!running_ && capture_ == nullptr) {
        restoreMute();
        return;
    }
    if (auto* timer = qobject_cast<QTimer*>(sampleTimer_)) {
        timer->stop();
    }
    stopCapture();
    restoreMute();
    running_ = false;
    endpointIds_.clear();
    ignoreSession_.clear();
}

void AcousticLagCalibrator::onSampleTimer()
{
    if (!running_ || router_ == nullptr || endpointIds_.isEmpty()) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = now - startedMs_;
    const int n = static_cast<int>(endpointIds_.size());
    const qint64 phaseMs = std::max<qint64>(2000, RuntimeLagAverager::kWindowMs / std::max(1, n));
    if (now - phaseStartedMs_ >= phaseMs && phaseIndex_ + 1 < n) {
        ++phaseIndex_;
        phaseStartedMs_ = now;
        applySolo(phaseIndex_);
    }

    const QVector<float> reference = snapshot(true);
    const QVector<float> mic = snapshot(false);
    if (const auto lag = estimateLagMs(reference, mic, sampleRateHz_)) {
        router_->ingestAcousticLagSample(endpointIds_.at(phaseIndex_), *lag);
        qCInfo(auralisAudio) << "AcousticLagSample endpoint=" << endpointIds_.at(phaseIndex_)
                             << "delayMs=" << *lag;
    }

    if (elapsed >= RuntimeLagAverager::kWindowMs) {
        if (auto* timer = qobject_cast<QTimer*>(sampleTimer_)) {
            timer->stop();
        }
        stopCapture();
        restoreMute();
        running_ = false;
        router_->applyRuntimeLagCompensation();
        calibratedSessions_.insert(sessionId_);
        status_ = QStringLiteral("Headset delay aligned from a 10s measurement");
        emit statusChanged();
        emit finished();
        qCInfo(auralisAudio) << "AcousticLagCalibrationFinished session=" << sessionId_;
        endpointIds_.clear();
    }
}

bool AcousticLagCalibrator::startCapture()
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(store_)
    return false;
#else
    const QString micName = findLaptopMicNodeName(store_);
    if (micName.isEmpty() || !virtualOutputSinkPresent(store_)) {
        qCWarning(auralisAudio) << "AcousticLagCaptureMissing mic=" << micName
                                << "virtualOutput=" << virtualOutputSinkPresent(store_);
        return false;
    }

    static std::once_flag pipeWireOnce;
    std::call_once(pipeWireOnce, []() { pw_init(nullptr, nullptr); });

    auto capture = std::make_unique<PipeWireLagCapture>();
    capture->rate = sampleRateHz_;
    capture->reference.owner = capture.get();
    capture->reference.reference = true;
    capture->mic.owner = capture.get();
    capture->loop = pw_thread_loop_new("auralis-lag-capture", nullptr);
    if (capture->loop == nullptr) {
        return false;
    }
    pw_thread_loop_lock(capture->loop);
    capture->context = pw_context_new(pw_thread_loop_get_loop(capture->loop), nullptr, 0);
    if (capture->context == nullptr) {
        pw_thread_loop_unlock(capture->loop);
        pw_thread_loop_destroy(capture->loop);
        return false;
    }
    capture->core = pw_context_connect(capture->context, nullptr, 0);
    if (capture->core == nullptr) {
        pw_context_destroy(capture->context);
        pw_thread_loop_unlock(capture->loop);
        pw_thread_loop_destroy(capture->loop);
        return false;
    }
    const QByteArray micUtf8 = micName.toUtf8();
    if (connectCaptureStream(
            capture->core,
            &capture->reference,
            "auralis.lag.reference",
            kAuralisVirtualSinkNodeName,
            true,
            sampleRateHz_)
        == nullptr) {
        qCWarning(auralisAudio) << "AcousticLagCaptureFailed stream=reference";
        pw_core_disconnect(capture->core);
        pw_context_destroy(capture->context);
        pw_thread_loop_unlock(capture->loop);
        pw_thread_loop_destroy(capture->loop);
        return false;
    }
    if (connectCaptureStream(
            capture->core,
            &capture->mic,
            "auralis.lag.mic",
            micUtf8.constData(),
            false,
            sampleRateHz_)
        == nullptr) {
        qCWarning(auralisAudio) << "AcousticLagCaptureFailed stream=mic node=" << micName;
        pw_stream_destroy(capture->reference.stream);
        pw_core_disconnect(capture->core);
        pw_context_destroy(capture->context);
        pw_thread_loop_unlock(capture->loop);
        pw_thread_loop_destroy(capture->loop);
        return false;
    }
    pw_thread_loop_unlock(capture->loop);
    capture_ = std::move(capture);
    if (pw_thread_loop_start(capture_->loop) < 0) {
        stopCapture();
        return false;
    }
    qCInfo(auralisAudio) << "AcousticLagCapture micNode=" << micName
                         << "reference=" << kAuralisVirtualSinkNodeName;
    return true;
#endif
}

void AcousticLagCalibrator::stopCapture()
{
#if defined(Q_OS_LINUX)
    if (capture_ == nullptr) {
        return;
    }
    if (capture_->loop != nullptr) {
        pw_thread_loop_stop(capture_->loop);
        pw_thread_loop_lock(capture_->loop);
        if (capture_->reference.stream != nullptr) {
            pw_stream_destroy(capture_->reference.stream);
            capture_->reference.stream = nullptr;
        }
        if (capture_->mic.stream != nullptr) {
            pw_stream_destroy(capture_->mic.stream);
            capture_->mic.stream = nullptr;
        }
        if (capture_->core != nullptr) {
            pw_core_disconnect(capture_->core);
            capture_->core = nullptr;
        }
        if (capture_->context != nullptr) {
            pw_context_destroy(capture_->context);
            capture_->context = nullptr;
        }
        pw_thread_loop_unlock(capture_->loop);
        pw_thread_loop_destroy(capture_->loop);
        capture_->loop = nullptr;
    }
#endif
    capture_.reset();
}

void AcousticLagCalibrator::applySolo(int index)
{
    if (backend_ == nullptr || endpoints_ == nullptr || index < 0 || index >= endpointIds_.size()) {
        return;
    }
    for (int i = 0; i < endpointIds_.size(); ++i) {
        const AudioEndpoint* endpoint = endpoints_->findById(endpointIds_.at(i));
        if (endpoint == nullptr || endpoint->pipeWireObjectId == 0) {
            continue;
        }
        backend_->setNodeMuted(endpoint->pipeWireObjectId, i != index);
    }
}

void AcousticLagCalibrator::restoreMute()
{
    if (backend_ == nullptr || endpoints_ == nullptr) {
        return;
    }
    for (const QString& id : endpointIds_) {
        const AudioEndpoint* endpoint = endpoints_->findById(id);
        if (endpoint == nullptr || endpoint->pipeWireObjectId == 0) {
            continue;
        }
        backend_->setNodeMuted(endpoint->pipeWireObjectId, false);
    }
}

QVector<float> AcousticLagCalibrator::snapshot(bool reference) const
{
    if (capture_ == nullptr) {
        return {};
    }
#if defined(Q_OS_LINUX)
    QMutexLocker locker(&capture_->mutex);
    const QVector<float>& source = reference ? capture_->referenceSamples : capture_->micSamples;
    if (source.size() < sampleRateHz_) {
        return {};
    }
    return source.mid(source.size() - sampleRateHz_);
#else
    Q_UNUSED(reference)
    return {};
#endif
}

} // namespace auralis::audio
