#include <auralis/audio/PipeWireConnection.h>

#include <auralis/core/LoggingCategories.h>

#include <QHash>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <pipewire/pipewire.h>
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

} // namespace

struct PipeWireConnection::Impl {
    struct BoundProxy {
        Impl* impl = nullptr;
        quint32 globalId = 0;
        PipeWireObjectKind kind = PipeWireObjectKind::Unknown;
        pw_proxy* proxy = nullptr;
        spa_hook objectListener{};
        spa_hook proxyListener{};
    };

    EventHandler handler;
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    spa_hook coreListener{};
    spa_hook registryListener{};
    QHash<quint32, BoundProxy*> proxies;
    int pendingSync = 0;
    bool started = false;
    bool stopping = false;

    static const pw_core_events kCoreEvents;
    static const pw_registry_events kRegistryEvents;
    static const pw_proxy_events kProxyEvents;
    static const pw_node_events kNodeEvents;
    static const pw_device_events kDeviceEvents;

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

    void destroyProxy(quint32 globalId)
    {
        BoundProxy* bound = proxies.take(globalId);
        if (bound == nullptr) {
            return;
        }
        if (bound->proxy != nullptr) {
            spa_hook_remove(&bound->objectListener);
            spa_hook_remove(&bound->proxyListener);
            pw_proxy_destroy(bound->proxy);
            bound->proxy = nullptr;
        }
        delete bound;
    }

    void destroyAllProxies()
    {
        const QList<quint32> ids = proxies.keys();
        for (quint32 id : ids) {
            destroyProxy(id);
        }
    }

    void bindIfNeeded(quint32 id, const char* type, uint32_t version, PipeWireObjectKind kind)
    {
        if (registry == nullptr || proxies.contains(id)) {
            return;
        }
        if (kind != PipeWireObjectKind::Device && kind != PipeWireObjectKind::Node) {
            return;
        }

        const uint32_t supported = kind == PipeWireObjectKind::Device ? PW_VERSION_DEVICE : PW_VERSION_NODE;
        auto* bound = new BoundProxy;
        bound->impl = this;
        bound->globalId = id;
        bound->kind = kind;
        bound->proxy = static_cast<pw_proxy*>(
            pw_registry_bind(registry, id, type, cappedVersion(version, supported), 0));
        if (bound->proxy == nullptr) {
            delete bound;
            qCWarning(auralisAudio) << "PipeWire BindingFailed id=" << id << "type=" << type;
            return;
        }

        pw_proxy_add_listener(bound->proxy, &bound->proxyListener, &kProxyEvents, bound);
        if (kind == PipeWireObjectKind::Node) {
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kNodeEvents, bound);
        } else {
            pw_proxy_add_object_listener(bound->proxy, &bound->objectListener, &kDeviceEvents, bound);
        }
        proxies.insert(id, bound);
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
        [](void* data, uint32_t, int, int res, const char* message) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr || impl->stopping) {
                return;
            }
            const QString text = QStringLiteral("PipeWire core error %1: %2")
                                     .arg(res)
                                     .arg(QString::fromUtf8(message != nullptr ? message : strerror(res)));
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
    .bound = nullptr,
    .removed =
        [](void* data) {
            auto* bound = static_cast<Impl::BoundProxy*>(data);
            if (bound == nullptr || bound->proxy == nullptr) {
                return;
            }
            pw_proxy_destroy(bound->proxy);
            bound->proxy = nullptr;
        },
    .done = nullptr,
    .error = nullptr,
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
        return true;
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
        return true;
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

} // namespace auralis::audio
