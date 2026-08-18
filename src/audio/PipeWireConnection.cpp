#include <auralis/audio/PipeWireConnection.h>

#include <auralis/core/LoggingCategories.h>

#include <QHash>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <pipewire/pipewire.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#pragma GCC diagnostic pop

namespace auralis::audio {
namespace {

std::once_flag gPipeWireInitOnce;

void ensurePipeWireInitialized()
{
    std::call_once(gPipeWireInitOnce, []() { pw_init(nullptr, nullptr); });
}

PipeWireProperties copyDict(const spa_dict* dict)
{
    QHash<QString, QString> values;
    if (dict == nullptr || dict->items == nullptr) {
        return PipeWireProperties::fromHash(std::move(values));
    }
    for (uint32_t i = 0; i < dict->n_items; ++i) {
        const spa_dict_item& item = dict->items[i];
        if (item.key == nullptr || item.value == nullptr) {
            continue;
        }
        values.insert(QString::fromUtf8(item.key), QString::fromUtf8(item.value));
    }
    return PipeWireProperties::fromHash(std::move(values));
}

uint32_t cappedVersion(uint32_t advertised, uint32_t supported)
{
    return std::min(advertised, supported);
}

bool isLostPipeWireConnection(int res)
{
    switch (res < 0 ? -res : res) {
    case EPIPE:
    case ECONNRESET:
    case ECONNABORTED:
    case ENOTCONN:
    case ESHUTDOWN:
    case EIO:
        return true;
    default:
        return false;
    }
}

QString linkStateName(pw_link_state state)
{
    switch (state) {
    case PW_LINK_STATE_ERROR:
        return QStringLiteral("error");
    case PW_LINK_STATE_UNLINKED:
        return QStringLiteral("unlinked");
    case PW_LINK_STATE_INIT:
        return QStringLiteral("init");
    case PW_LINK_STATE_NEGOTIATING:
        return QStringLiteral("negotiating");
    case PW_LINK_STATE_ALLOCATING:
        return QStringLiteral("allocating");
    case PW_LINK_STATE_PAUSED:
        return QStringLiteral("paused");
    case PW_LINK_STATE_ACTIVE:
        return QStringLiteral("active");
    }
    return QStringLiteral("unknown");
}

uint32_t supportedVersion(PipeWireObjectKind kind)
{
    switch (kind) {
    case PipeWireObjectKind::Device:
        return PW_VERSION_DEVICE;
    case PipeWireObjectKind::Node:
        return PW_VERSION_NODE;
    case PipeWireObjectKind::Port:
        return PW_VERSION_PORT;
    case PipeWireObjectKind::Link:
        return PW_VERSION_LINK;
    default:
        return 0;
    }
}

} // namespace

struct PipeWireConnection::Impl {
    struct BoundProxy {
        Impl* impl = nullptr;
        quint32 globalId = 0;
        PipeWireObjectKind kind = PipeWireObjectKind::Unknown;
        pw_proxy* proxy = nullptr;
        spa_hook objectListener{};
        spa_hook proxyListener{};
        bool created = false;
    };

    EventHandler handler;
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    spa_hook coreListener{};
    spa_hook registryListener{};
    QHash<quint32, BoundProxy*> proxies;
    QVector<BoundProxy*> pendingCreated;
    QSet<quint32> createdLinkIds;
    int pendingSync = 0;
    bool started = false;
    bool stopping = false;

    static const pw_core_events kCoreEvents;
    static const pw_registry_events kRegistryEvents;
    static const pw_proxy_events kProxyEvents;
    static const pw_node_events kNodeEvents;
    static const pw_device_events kDeviceEvents;
    static const pw_port_events kPortEvents;
    static const pw_link_events kLinkEvents;

    void emitEvent(const PipeWireClientEvent& event)
    {
        if (handler && !stopping) {
            handler(event);
        }
    }

    void emitState(PipeWireConnectionState state, const QString& error = {})
    {
        PipeWireClientEvent event;
        event.type = PipeWireClientEvent::Type::StateChanged;
        event.state = state;
        event.error = error;
        emitEvent(event);
    }

    void emitSnapshot(PipeWireClientEvent::Type type, quint32 id, const char* interfaceType, const spa_dict* props)
    {
        PipeWireClientEvent event;
        event.type = type;
        event.snapshot.globalId = id;
        event.snapshot.interfaceType = QString::fromUtf8(interfaceType != nullptr ? interfaceType : "");
        event.snapshot.kind = kindFromInterfaceType(event.snapshot.interfaceType);
        event.snapshot.properties = copyDict(props);
        emitEvent(event);
    }

    void emitLinkInfo(quint32 id, const pw_link_info* info)
    {
        QHash<QString, QString> values = copyDict(info != nullptr ? info->props : nullptr).toHash();
        if (info != nullptr) {
            values.insert(QStringLiteral("link.output.node"), QString::number(info->output_node_id));
            values.insert(QStringLiteral("link.output.port"), QString::number(info->output_port_id));
            values.insert(QStringLiteral("link.input.node"), QString::number(info->input_node_id));
            values.insert(QStringLiteral("link.input.port"), QString::number(info->input_port_id));
            values.insert(QStringLiteral("link.state"), linkStateName(info->state));
            if (info->error != nullptr) {
                values.insert(QStringLiteral("link.error"), QString::fromUtf8(info->error));
            }
        }
        PipeWireClientEvent event;
        event.type = PipeWireClientEvent::Type::GlobalUpdated;
        event.snapshot.globalId = id;
        event.snapshot.interfaceType = QString::fromUtf8(PW_TYPE_INTERFACE_Link);
        event.snapshot.kind = PipeWireObjectKind::Link;
        event.snapshot.properties = PipeWireProperties::fromHash(std::move(values));
        emitEvent(event);
    }

    void addObjectListener(BoundProxy* bound)
    {
        switch (bound->kind) {
        case PipeWireObjectKind::Node:
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kNodeEvents, bound);
            break;
        case PipeWireObjectKind::Device:
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kDeviceEvents, bound);
            break;
        case PipeWireObjectKind::Port:
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kPortEvents, bound);
            break;
        case PipeWireObjectKind::Link:
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kLinkEvents, bound);
            break;
        default:
            break;
        }
    }

    void destroyProxy(quint32 globalId)
    {
        BoundProxy* bound = proxies.take(globalId);
        if (bound == nullptr) {
            return;
        }
        createdLinkIds.remove(globalId);
        spa_hook_remove(&bound->objectListener);
        spa_hook_remove(&bound->proxyListener);
        pw_proxy* proxy = bound->proxy;
        bound->proxy = nullptr;
        bound->impl = nullptr;
        if (proxy != nullptr) {
            pw_proxy_destroy(proxy);
        }
    }

    void destroyPending(BoundProxy* bound)
    {
        pendingCreated.removeAll(bound);
        spa_hook_remove(&bound->objectListener);
        spa_hook_remove(&bound->proxyListener);
        pw_proxy* proxy = bound->proxy;
        bound->proxy = nullptr;
        bound->impl = nullptr;
        if (proxy != nullptr) {
            pw_proxy_destroy(proxy);
        }
    }

    void destroyAllProxies()
    {
        const QList<quint32> created = createdLinkIds.values();
        for (quint32 id : created) {
            destroyProxy(id);
        }
        const QList<quint32> ids = proxies.keys();
        for (quint32 id : ids) {
            destroyProxy(id);
        }
        const QVector<BoundProxy*> pending = pendingCreated;
        for (BoundProxy* bound : pending) {
            destroyPending(bound);
        }
    }

    void bindIfNeeded(quint32 id, const char* type, uint32_t version, PipeWireObjectKind kind)
    {
        if (registry == nullptr || proxies.contains(id)) {
            return;
        }
        if (kind != PipeWireObjectKind::Device && kind != PipeWireObjectKind::Node && kind != PipeWireObjectKind::Port
            && kind != PipeWireObjectKind::Link) {
            return;
        }

        pw_proxy* proxy = static_cast<pw_proxy*>(pw_registry_bind(
            registry, id, type, cappedVersion(version, supportedVersion(kind)), sizeof(BoundProxy)));
        if (proxy == nullptr) {
            qCWarning(auralisAudio) << "PipeWire BindingFailed id=" << id << "type=" << type;
            return;
        }

        auto* bound = static_cast<BoundProxy*>(pw_proxy_get_user_data(proxy));
        bound->impl = this;
        bound->globalId = id;
        bound->kind = kind;
        bound->proxy = proxy;
        bound->created = false;
        pw_proxy_add_listener(bound->proxy, &bound->proxyListener, &kProxyEvents, bound);
        addObjectListener(bound);
        proxies.insert(id, bound);
    }

    bool applyNodeProps(quint32 nodeId, const std::optional<float>& volume, const std::optional<bool>& mute)
    {
        if (loop == nullptr || !started || stopping) {
            return false;
        }
        BoundProxy* bound = proxies.value(nodeId);
        if (bound == nullptr || bound->kind != PipeWireObjectKind::Node || bound->proxy == nullptr) {
            return false;
        }

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_pod_frame frame{};
        spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
        if (volume.has_value()) {
            spa_pod_builder_prop(&builder, SPA_PROP_volume, 0);
            spa_pod_builder_float(&builder, *volume);
        }
        if (mute.has_value()) {
            spa_pod_builder_prop(&builder, SPA_PROP_mute, 0);
            spa_pod_builder_bool(&builder, *mute);
        }
        const spa_pod* pod = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame));
        if (pod == nullptr) {
            return false;
        }
        pw_thread_loop_lock(loop);
        const int rc = pw_node_set_param(reinterpret_cast<pw_node*>(bound->proxy), SPA_PARAM_Props, 0, pod);
        pw_thread_loop_unlock(loop);
        return rc >= 0;
    }
};

const pw_core_events PipeWireConnection::Impl::kCoreEvents = {
    .version = PW_VERSION_CORE_EVENTS,
    .info = nullptr,
    .done =
        [](void* data, uint32_t id, int seq) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr || impl->stopping) {
                return;
            }
            if (id == PW_ID_CORE && seq == impl->pendingSync) {
                PipeWireClientEvent event;
                event.type = PipeWireClientEvent::Type::InitialSyncDone;
                impl->emitEvent(event);
            }
        },
    .ping = nullptr,
    .error =
        [](void* data, uint32_t id, int, int res, const char* message) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr || impl->stopping) {
                return;
            }
            const QString text = QStringLiteral("id=%1 res=%2 %3")
                                     .arg(id)
                                     .arg(res)
                                     .arg(QString::fromUtf8(message != nullptr ? message : strerror(res)));
            if (id != PW_ID_CORE) {
                impl->destroyProxy(id);
            }
            if (!isLostPipeWireConnection(res)) {
                qCDebug(auralisAudio) << "PipeWire object error" << text;
                return;
            }
            qCCritical(auralisAudio) << "PipeWire ConnectionFailed" << text;
            impl->emitState(PipeWireConnectionState::Error, text);
        },
    .remove_id = nullptr,
    .bound_id = nullptr,
    .add_mem = nullptr,
    .remove_mem = nullptr,
    .bound_props = nullptr,
};

const pw_registry_events PipeWireConnection::Impl::kRegistryEvents = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global =
        [](void* data, uint32_t id, uint32_t, const char* type, uint32_t version, const spa_dict* props) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr || impl->stopping || type == nullptr) {
                return;
            }
            const PipeWireObjectKind kind = kindFromInterfaceType(QString::fromUtf8(type));
            impl->emitSnapshot(PipeWireClientEvent::Type::GlobalAdded, id, type, props);
            impl->bindIfNeeded(id, type, version, kind);
        },
    .global_remove =
        [](void* data, uint32_t id) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr || impl->stopping) {
                return;
            }
            impl->destroyProxy(id);
            PipeWireClientEvent event;
            event.type = PipeWireClientEvent::Type::GlobalRemoved;
            event.removedId = id;
            impl->emitEvent(event);
        },
};

const pw_proxy_events PipeWireConnection::Impl::kProxyEvents = {
    .version = PW_VERSION_PROXY_EVENTS,
    .destroy = nullptr,
    .bound =
        [](void* data, uint32_t global) {
            auto* bound = static_cast<Impl::BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr) {
                return;
            }
            bound->globalId = global;
            if (bound->created) {
                bound->impl->pendingCreated.removeAll(bound);
                bound->impl->proxies.insert(global, bound);
                bound->impl->createdLinkIds.insert(global);
            }
        },
    .removed =
        [](void* data) {
            auto* bound = static_cast<Impl::BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr) {
                return;
            }
            if (bound->globalId != 0) {
                bound->impl->destroyProxy(bound->globalId);
            } else {
                bound->impl->destroyPending(bound);
            }
        },
    .done = nullptr,
    .error =
        [](void* data, int, int res, const char* message) {
            auto* bound = static_cast<Impl::BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr) {
                return;
            }
            qCDebug(auralisAudio) << "PipeWire proxy error id=" << bound->globalId << "res=" << res
                                 << QString::fromUtf8(message != nullptr ? message : "");
            if (bound->globalId != 0) {
                bound->impl->destroyProxy(bound->globalId);
            } else {
                bound->impl->destroyPending(bound);
            }
        },
    .bound_props = nullptr,
};

const pw_node_events PipeWireConnection::Impl::kNodeEvents = {
    .version = PW_VERSION_NODE_EVENTS,
    .info =
        [](void* data, const pw_node_info* info) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || info == nullptr || bound->impl->stopping) {
                return;
            }
            bound->impl->emitSnapshot(
                PipeWireClientEvent::Type::GlobalUpdated,
                bound->globalId,
                PW_TYPE_INTERFACE_Node,
                info->props);
        },
    .param = nullptr,
};

const pw_device_events PipeWireConnection::Impl::kDeviceEvents = {
    .version = PW_VERSION_DEVICE_EVENTS,
    .info =
        [](void* data, const pw_device_info* info) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || info == nullptr || bound->impl->stopping) {
                return;
            }
            bound->impl->emitSnapshot(
                PipeWireClientEvent::Type::GlobalUpdated,
                bound->globalId,
                PW_TYPE_INTERFACE_Device,
                info->props);
        },
    .param = nullptr,
};

const pw_port_events PipeWireConnection::Impl::kPortEvents = {
    .version = PW_VERSION_PORT_EVENTS,
    .info =
        [](void* data, const pw_port_info* info) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || info == nullptr || bound->impl->stopping) {
                return;
            }
            bound->impl->emitSnapshot(
                PipeWireClientEvent::Type::GlobalUpdated,
                bound->globalId,
                PW_TYPE_INTERFACE_Port,
                info->props);
        },
    .param = nullptr,
};

const pw_link_events PipeWireConnection::Impl::kLinkEvents = {
    .version = PW_VERSION_LINK_EVENTS,
    .info =
        [](void* data, const pw_link_info* info) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || info == nullptr || bound->impl->stopping) {
                return;
            }
            bound->impl->emitLinkInfo(bound->globalId, info);
        },
};

PipeWireConnection::PipeWireConnection() = default;

PipeWireConnection::~PipeWireConnection()
{
    stop();
}

bool PipeWireConnection::isStarted() const noexcept
{
    return impl_ != nullptr && impl_->started;
}

bool PipeWireConnection::start(EventHandler handler)
{
    stop();
    ensurePipeWireInitialized();

    impl_ = new Impl;
    impl_->handler = std::move(handler);
    impl_->emitState(PipeWireConnectionState::Starting);
    qCInfo(auralisAudio) << "PipeWire Connecting";

    impl_->loop = pw_thread_loop_new("auralis-pipewire", nullptr);
    if (impl_->loop == nullptr) {
        const QString error = QStringLiteral("Failed to create PipeWire thread loop");
        impl_->emitState(PipeWireConnectionState::Error, error);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    pw_thread_loop_lock(impl_->loop);
    impl_->context = pw_context_new(pw_thread_loop_get_loop(impl_->loop), nullptr, 0);
    if (impl_->context == nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        const QString error = QStringLiteral("Failed to create PipeWire context");
        impl_->emitState(PipeWireConnectionState::Error, error);
        pw_thread_loop_destroy(impl_->loop);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    impl_->core = pw_context_connect(impl_->context, nullptr, 0);
    if (impl_->core == nullptr) {
        const int saved = errno;
        pw_thread_loop_unlock(impl_->loop);
        const QString error = QStringLiteral("Failed to connect to PipeWire: %1").arg(QString::fromLocal8Bit(strerror(saved)));
        qCCritical(auralisAudio) << "PipeWire ConnectionFailed" << error;
        impl_->emitState(PipeWireConnectionState::Error, error);
        pw_context_destroy(impl_->context);
        pw_thread_loop_destroy(impl_->loop);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    pw_core_add_listener(impl_->core, &impl_->coreListener, &Impl::kCoreEvents, impl_);
    impl_->registry = pw_core_get_registry(impl_->core, PW_VERSION_REGISTRY, 0);
    if (impl_->registry == nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        const QString error = QStringLiteral("Failed to get PipeWire registry");
        impl_->emitState(PipeWireConnectionState::Error, error);
        pw_core_disconnect(impl_->core);
        pw_context_destroy(impl_->context);
        pw_thread_loop_destroy(impl_->loop);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    pw_registry_add_listener(impl_->registry, &impl_->registryListener, &Impl::kRegistryEvents, impl_);
    impl_->pendingSync = pw_core_sync(impl_->core, PW_ID_CORE, 0);
    pw_thread_loop_unlock(impl_->loop);

    if (pw_thread_loop_start(impl_->loop) < 0) {
        const QString error = QStringLiteral("Failed to start PipeWire thread loop");
        impl_->emitState(PipeWireConnectionState::Error, error);
        stop();
        return false;
    }

    impl_->started = true;
    impl_->emitState(PipeWireConnectionState::Connected);
    qCInfo(auralisAudio) << "PipeWire Connected";
    return true;
}

void PipeWireConnection::stop()
{
    if (impl_ == nullptr) {
        return;
    }

    impl_->stopping = true;
    impl_->emitState(PipeWireConnectionState::Stopping);

    if (impl_->loop != nullptr) {
        pw_thread_loop_stop(impl_->loop);
        pw_thread_loop_lock(impl_->loop);
        impl_->destroyAllProxies();
        if (impl_->registry != nullptr) {
            spa_hook_remove(&impl_->registryListener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(impl_->registry));
            impl_->registry = nullptr;
        }
        if (impl_->core != nullptr) {
            spa_hook_remove(&impl_->coreListener);
            pw_core_disconnect(impl_->core);
            impl_->core = nullptr;
        }
        if (impl_->context != nullptr) {
            pw_context_destroy(impl_->context);
            impl_->context = nullptr;
        }
        pw_thread_loop_unlock(impl_->loop);
        pw_thread_loop_destroy(impl_->loop);
        impl_->loop = nullptr;
    }

    delete impl_;
    impl_ = nullptr;
}

std::optional<quint32> PipeWireConnection::createLink(
    quint32 outputNode,
    quint32 outputPort,
    quint32 inputNode,
    quint32 inputPort,
    const QString& routeId,
    const QHash<QString, QString>& extraProps)
{
    if (impl_ == nullptr || impl_->core == nullptr || impl_->loop == nullptr || !impl_->started || impl_->stopping) {
        return std::nullopt;
    }

    QVector<QByteArray> storage;
    auto add = [&](const QByteArray& key, const QByteArray& value) {
        storage.push_back(key);
        storage.push_back(value);
    };
    add(PW_KEY_LINK_OUTPUT_NODE, QByteArray::number(outputNode));
    add(PW_KEY_LINK_OUTPUT_PORT, QByteArray::number(outputPort));
    add(PW_KEY_LINK_INPUT_NODE, QByteArray::number(inputNode));
    add(PW_KEY_LINK_INPUT_PORT, QByteArray::number(inputPort));
    add(PW_KEY_APP_NAME, QByteArray("Auralis"));
    if (!routeId.isEmpty()) {
        add(QByteArray("auralis.route.id"), routeId.toUtf8());
    }
    for (auto it = extraProps.constBegin(); it != extraProps.constEnd(); ++it) {
        add(it.key().toUtf8(), it.value().toUtf8());
    }

    QVector<spa_dict_item> items;
    items.reserve(storage.size() / 2);
    for (int i = 0; i + 1 < storage.size(); i += 2) {
        items.push_back(spa_dict_item{storage.at(i).constData(), storage.at(i + 1).constData()});
    }
    spa_dict dict{};
    dict.items = items.constData();
    dict.n_items = static_cast<uint32_t>(items.size());

    pw_thread_loop_lock(impl_->loop);
    pw_proxy* proxy = static_cast<pw_proxy*>(pw_core_create_object(
        impl_->core,
        "link-factory",
        PW_TYPE_INTERFACE_Link,
        PW_VERSION_LINK,
        &dict,
        sizeof(Impl::BoundProxy)));
    if (proxy == nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        qCWarning(auralisAudio) << "PipeWire LinkCreateFailed"
                                << "out=" << outputNode << ":" << outputPort << "in=" << inputNode << ":" << inputPort;
        return std::nullopt;
    }

    auto* bound = static_cast<Impl::BoundProxy*>(pw_proxy_get_user_data(proxy));
    bound->impl = impl_;
    bound->globalId = 0;
    bound->kind = PipeWireObjectKind::Link;
    bound->proxy = proxy;
    bound->created = true;
    pw_proxy_add_listener(bound->proxy, &bound->proxyListener, &Impl::kProxyEvents, bound);
    impl_->addObjectListener(bound);
    impl_->pendingCreated.push_back(bound);
    const uint32_t boundId = pw_proxy_get_bound_id(proxy);
    if (boundId != SPA_ID_INVALID && boundId != 0) {
        bound->globalId = boundId;
        impl_->pendingCreated.removeAll(bound);
        impl_->proxies.insert(boundId, bound);
        impl_->createdLinkIds.insert(boundId);
    }
    pw_thread_loop_unlock(impl_->loop);

    qCInfo(auralisAudio) << "PipeWire LinkCreateRequested route=" << routeId << "out=" << outputNode << ":" << outputPort
                         << "in=" << inputNode << ":" << inputPort << "id=" << bound->globalId;
    if (bound->globalId == 0) {
        return quint32{0};
    }
    return bound->globalId;
}

bool PipeWireConnection::destroyOwnedLink(quint32 globalId)
{
    if (impl_ == nullptr || impl_->loop == nullptr || globalId == 0) {
        return false;
    }
    if (!impl_->createdLinkIds.contains(globalId)) {
        return false;
    }
    pw_thread_loop_lock(impl_->loop);
    impl_->destroyProxy(globalId);
    pw_thread_loop_unlock(impl_->loop);
    qCInfo(auralisAudio) << "PipeWire LinkDestroyed id=" << globalId;
    return true;
}

bool PipeWireConnection::setNodeVolume(quint32 nodeId, double volume)
{
    if (impl_ == nullptr) {
        return false;
    }
    const float clamped = static_cast<float>(std::clamp(volume, 0.0, 1.0));
    return impl_->applyNodeProps(nodeId, clamped, std::nullopt);
}

bool PipeWireConnection::setNodeMuted(quint32 nodeId, bool muted)
{
    if (impl_ == nullptr) {
        return false;
    }
    return impl_->applyNodeProps(nodeId, std::nullopt, muted);
}

bool PipeWireConnection::volumeSupported(quint32 nodeId) const
{
    if (impl_ == nullptr) {
        return false;
    }
    const Impl::BoundProxy* bound = impl_->proxies.value(nodeId);
    return bound != nullptr && bound->kind == PipeWireObjectKind::Node;
}

} // namespace auralis::audio
