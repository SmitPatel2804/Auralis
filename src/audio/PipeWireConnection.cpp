#include <auralis/audio/PipeWireConnection.h>

#include <auralis/audio/PipeWireVirtualOutput.h>
#include <auralis/core/LoggingCategories.h>

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/impl-module.h>
#include <spa/param/latency-utils.h>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/utils/dict.h>
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
    case PipeWireObjectKind::Metadata:
        return PW_VERSION_METADATA;
    default:
        return 0;
    }
}

} // namespace

struct PipeWireConnection::Impl {
    struct BoundProxy {
        Impl* impl = nullptr;
        quint32 globalId = 0;
        quint64 ownershipToken = 0;
        PipeWireObjectKind kind = PipeWireObjectKind::Unknown;
        pw_proxy* proxy = nullptr;
        spa_hook objectListener{};
        spa_hook proxyListener{};
        bool created = false;
        bool propsWritable = false;
    };

    EventHandler handler;
    LinkErrorHandler linkErrorHandler;
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    spa_hook coreListener{};
    spa_hook registryListener{};
    pw_impl_module* virtualOutputModule = nullptr;
    spa_hook virtualOutputModuleListener{};
    pw_impl_module* fanoutModule = nullptr;
    spa_hook fanoutModuleListener{};
    QStringList fanoutSinkNames;
    struct DelayBridge {
        Impl* impl = nullptr;
        QString endpointId;
        QString destNodeName;
        double delaySec = 0;
        pw_impl_module* module = nullptr;
        spa_hook listener{};
    };
    QHash<QString, DelayBridge*> delayBridges;
    QHash<quint32, BoundProxy*> proxies;
    QHash<quint64, BoundProxy*> ownedByToken;
    QHash<quint64, quint32> tokenToGlobalId;
    QVector<BoundProxy*> pendingCreated;
    QSet<quint32> createdLinkIds;
    quint32 defaultMetadataId = 0;
    quint64 nextOwnershipToken = 1;
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
    static const pw_metadata_events kMetadataEvents;
    static const pw_impl_module_events kModuleEvents;
    static const pw_impl_module_events kFanoutModuleEvents;
    static const pw_impl_module_events kDelayModuleEvents;

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

    void emitMetadata(
        BoundProxy* bound,
        uint32_t subject,
        const char* key,
        const char* type,
        const char* value)
    {
        if (bound == nullptr || key == nullptr) {
            return;
        }
        PipeWireClientEvent event;
        event.type = PipeWireClientEvent::Type::MetadataChanged;
        event.metadataSubject = subject;
        event.metadataName = QStringLiteral("default");
        event.metadataKey = QString::fromUtf8(key);
        event.metadataType = QString::fromUtf8(type != nullptr ? type : "");
        event.metadataValue = QString::fromUtf8(value != nullptr ? value : "");
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
        case PipeWireObjectKind::Metadata:
            pw_metadata_add_listener(
                reinterpret_cast<pw_metadata*>(bound->proxy),
                &bound->objectListener,
                &kMetadataEvents,
                bound);
            break;
        default:
            break;
        }
    }

    void forgetOwned(BoundProxy* bound)
    {
        if (bound == nullptr || bound->ownershipToken == 0) {
            return;
        }
        ownedByToken.remove(bound->ownershipToken);
        tokenToGlobalId.remove(bound->ownershipToken);
        bound->ownershipToken = 0;
    }

    void destroyProxy(quint32 globalId)
    {
        BoundProxy* bound = proxies.take(globalId);
        if (bound == nullptr) {
            return;
        }
        createdLinkIds.remove(globalId);
        if (defaultMetadataId == globalId) {
            defaultMetadataId = 0;
        }
        forgetOwned(bound);
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
        forgetOwned(bound);
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

    void bindIfNeeded(quint32 id, const char* type, uint32_t version, PipeWireObjectKind kind, const spa_dict* props)
    {
        if (registry == nullptr || proxies.contains(id) || createdLinkIds.contains(id)) {
            return;
        }
        if (kind == PipeWireObjectKind::Link && props != nullptr
            && spa_dict_lookup(props, "auralis.route.id") != nullptr) {
            return;
        }
        if (kind != PipeWireObjectKind::Device && kind != PipeWireObjectKind::Node && kind != PipeWireObjectKind::Port
            && kind != PipeWireObjectKind::Link && kind != PipeWireObjectKind::Metadata) {
            return;
        }
        if (kind == PipeWireObjectKind::Metadata) {
            const char* metadataName = props != nullptr ? spa_dict_lookup(props, PW_KEY_METADATA_NAME) : nullptr;
            if (metadataName == nullptr || std::strcmp(metadataName, "default") != 0) {
                return;
            }
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
        bound->ownershipToken = 0;
        bound->propsWritable = false;
        pw_proxy_add_listener(bound->proxy, &bound->proxyListener, &kProxyEvents, bound);
        addObjectListener(bound);
        proxies.insert(id, bound);
        if (kind == PipeWireObjectKind::Metadata) {
            defaultMetadataId = id;
        }
    }

    bool applyNodeProps(quint32 nodeId, const std::optional<float>& volume, const std::optional<bool>& mute)
    {
        return applyNodeProps(nodeId, volume, mute, std::nullopt);
    }

    bool applyNodeProps(
        quint32 nodeId,
        const std::optional<float>& volume,
        const std::optional<bool>& mute,
        const std::optional<float>& delaySec)
    {
        if (loop == nullptr || !started || stopping) {
            return false;
        }
        pw_thread_loop_lock(loop);
        BoundProxy* bound = proxies.value(nodeId);
        if (bound == nullptr || bound->kind != PipeWireObjectKind::Node || bound->proxy == nullptr) {
            pw_thread_loop_unlock(loop);
            return false;
        }

        uint8_t buffer[2048];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_pod_frame frame{};
        spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
        if (volume.has_value()) {
            spa_pod_builder_prop(&builder, SPA_PROP_volume, 0);
            spa_pod_builder_float(&builder, *volume);
            // BlueZ sinks often ignore the scalar and honor channelVolumes.
            const float channels[2] = {*volume, *volume};
            spa_pod_builder_prop(&builder, SPA_PROP_channelVolumes, 0);
            spa_pod_builder_array(&builder, sizeof(float), SPA_TYPE_Float, 2, channels);
        }
        if (mute.has_value()) {
            spa_pod_builder_prop(&builder, SPA_PROP_mute, 0);
            spa_pod_builder_bool(&builder, *mute);
        }
        if (delaySec.has_value()) {
            const float clamped = std::clamp(*delaySec, 0.0f, 0.5f);
            QByteArray graph;
            if (clamped > 0.0005f) {
                graph = "{ nodes = [ { type = builtin name = auralis_delay label = delay "
                        "config = { \"max-delay\" = 1.0 } control = { \"Delay (s)\" = ";
                graph += QByteArray::number(static_cast<double>(clamped), 'f', 4);
                graph += " } } ] }";
            }
            spa_pod_builder_prop(&builder, SPA_PROP_params, 0);
            spa_pod_frame paramsFrame{};
            spa_pod_builder_push_struct(&builder, &paramsFrame);
            spa_pod_builder_string(&builder, "audioconvert.filter-graph");
            spa_pod_builder_string(&builder, graph.constData());
            spa_pod_builder_pop(&builder, &paramsFrame);
        }
        const spa_pod* pod = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame));
        if (pod == nullptr) {
            pw_thread_loop_unlock(loop);
            return false;
        }
        const int rc = pw_node_set_param(reinterpret_cast<pw_node*>(bound->proxy), SPA_PARAM_Props, 0, pod);
        pw_thread_loop_unlock(loop);
        return rc >= 0;
    }

    void destroyFanoutLocked()
    {
        fanoutSinkNames.clear();
        if (fanoutModule == nullptr) {
            return;
        }
        pw_impl_module* module = fanoutModule;
        fanoutModule = nullptr;
        spa_hook_remove(&fanoutModuleListener);
        pw_impl_module_destroy(module);
    }

    void destroyDelayBridgeLocked(const QString& endpointId)
    {
        DelayBridge* bridge = delayBridges.take(endpointId);
        if (bridge == nullptr) {
            return;
        }
        if (bridge->module != nullptr) {
            spa_hook_remove(&bridge->listener);
            pw_impl_module* module = bridge->module;
            bridge->module = nullptr;
            bridge->impl = nullptr;
            pw_impl_module_destroy(module);
        }
        delete bridge;
    }

    void destroyAllDelayBridgesLocked()
    {
        const QList<QString> ids = delayBridges.keys();
        for (const QString& id : ids) {
            destroyDelayBridgeLocked(id);
        }
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
            impl->bindIfNeeded(id, type, version, kind, props);
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
                if (bound->ownershipToken != 0) {
                    bound->impl->ownedByToken.insert(bound->ownershipToken, bound);
                    bound->impl->tokenToGlobalId.insert(bound->ownershipToken, global);
                }
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
            if (bound->created && bound->ownershipToken != 0 && bound->impl->linkErrorHandler) {
                bound->impl->linkErrorHandler(
                    bound->ownershipToken,
                    res,
                    QString::fromUtf8(message != nullptr ? message : ""));
            }
            if (bound->globalId != 0) {
                bound->impl->destroyProxy(bound->globalId);
            } else {
                bound->impl->destroyPending(bound);
            }
        },
};

const pw_node_events PipeWireConnection::Impl::kNodeEvents = {
    .version = PW_VERSION_NODE_EVENTS,
    .info =
        [](void* data, const pw_node_info* info) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || info == nullptr || bound->impl->stopping) {
                return;
            }
            bound->propsWritable = false;
            if (info->params != nullptr) {
                bool enumLatency = false;
                for (uint32_t i = 0; i < info->n_params; ++i) {
                    if (info->params[i].id == SPA_PARAM_Props
                        && (info->params[i].flags & SPA_PARAM_INFO_WRITE) != 0) {
                        bound->propsWritable = true;
                    }
                    if (info->params[i].id == SPA_PARAM_Latency) {
                        enumLatency = true;
                    }
                }
                if (enumLatency && bound->proxy != nullptr) {
                    pw_node_enum_params(
                        reinterpret_cast<pw_node*>(bound->proxy),
                        0,
                        SPA_PARAM_Latency,
                        0,
                        std::numeric_limits<uint32_t>::max(),
                        nullptr);
                }
            }
            bound->impl->emitSnapshot(
                PipeWireClientEvent::Type::GlobalUpdated,
                bound->globalId,
                PW_TYPE_INTERFACE_Node,
                info->props);
        },
    .param =
        [](void* data, int, uint32_t id, uint32_t, uint32_t, const spa_pod* param) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || bound->impl->stopping || param == nullptr) {
                return;
            }
            if (id != SPA_PARAM_Latency) {
                return;
            }
            spa_latency_info latency{};
            if (spa_latency_parse(param, &latency) < 0 || latency.direction != SPA_DIRECTION_INPUT) {
                return;
            }
            const int64_t ns = latency.min_ns > 0 ? latency.min_ns : latency.max_ns;
            if (ns <= 0) {
                return;
            }
            QHash<QString, QString> values;
            values.insert(QStringLiteral("auralis.latency.input.ns"), QString::number(static_cast<qint64>(ns)));
            PipeWireClientEvent event;
            event.type = PipeWireClientEvent::Type::GlobalUpdated;
            event.snapshot.globalId = bound->globalId;
            event.snapshot.interfaceType = QString::fromUtf8(PW_TYPE_INTERFACE_Node);
            event.snapshot.kind = PipeWireObjectKind::Node;
            event.snapshot.properties = PipeWireProperties::fromHash(std::move(values));
            bound->impl->emitEvent(event);
        },
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

const pw_metadata_events PipeWireConnection::Impl::kMetadataEvents = {
    .version = PW_VERSION_METADATA_EVENTS,
    .property =
        [](void* data, uint32_t subject, const char* key, const char* type, const char* value) {
            auto* bound = static_cast<BoundProxy*>(data);
            if (bound == nullptr || bound->impl == nullptr || bound->impl->stopping) {
                return 0;
            }
            bound->impl->emitMetadata(bound, subject, key, type, value);
            return 0;
        },
};

const pw_impl_module_events PipeWireConnection::Impl::kModuleEvents = {
    .version = PW_VERSION_IMPL_MODULE_EVENTS,
    .destroy =
        [](void* data) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr) {
                return;
            }
            // Modules can schedule their own destruction when initialization
            // fails. Never leave the public ownership query with a dangling
            // pw_impl_module pointer in that case.
            spa_hook_remove(&impl->virtualOutputModuleListener);
            impl->virtualOutputModule = nullptr;
            if (!impl->stopping) {
                qCWarning(auralisAudio) << "PipeWire VirtualOutputModuleDestroyed unexpectedly";
            }
        },
    .free = nullptr,
    .initialized = nullptr,
    .registered = nullptr,
};

const pw_impl_module_events PipeWireConnection::Impl::kFanoutModuleEvents = {
    .version = PW_VERSION_IMPL_MODULE_EVENTS,
    .destroy =
        [](void* data) {
            auto* impl = static_cast<Impl*>(data);
            if (impl == nullptr) {
                return;
            }
            spa_hook_remove(&impl->fanoutModuleListener);
            impl->fanoutModule = nullptr;
            impl->fanoutSinkNames.clear();
            if (!impl->stopping) {
                qCWarning(auralisAudio) << "PipeWire SessionFanoutModuleDestroyed unexpectedly";
            }
        },
    .free = nullptr,
    .initialized = nullptr,
    .registered = nullptr,
};

const pw_impl_module_events PipeWireConnection::Impl::kDelayModuleEvents = {
    .version = PW_VERSION_IMPL_MODULE_EVENTS,
    .destroy =
        [](void* data) {
            auto* bridge = static_cast<Impl::DelayBridge*>(data);
            if (bridge == nullptr) {
                return;
            }
            spa_hook_remove(&bridge->listener);
            bridge->module = nullptr;
            Impl* impl = bridge->impl;
            bridge->impl = nullptr;
            if (impl != nullptr && impl->delayBridges.value(bridge->endpointId) == bridge) {
                impl->delayBridges.remove(bridge->endpointId);
                if (!impl->stopping) {
                    qCWarning(auralisAudio) << "PipeWire DelayBridgeDestroyed unexpectedly endpoint="
                                            << bridge->endpointId;
                }
                delete bridge;
            }
        },
    .free = nullptr,
    .initialized = nullptr,
    .registered = nullptr,
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
    impl_->linkErrorHandler = linkErrorHandler_;
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

    impl_->emitState(PipeWireConnectionState::Stopping);
    impl_->stopping = true;

    if (impl_->loop != nullptr) {
        pw_thread_loop_stop(impl_->loop);
        pw_thread_loop_lock(impl_->loop);
        if (impl_->virtualOutputModule != nullptr) {
            pw_impl_module* module = impl_->virtualOutputModule;
            impl_->virtualOutputModule = nullptr;
            spa_hook_remove(&impl_->virtualOutputModuleListener);
            pw_impl_module_destroy(module);
        }
        impl_->destroyFanoutLocked();
        impl_->destroyAllDelayBridgesLocked();
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

bool PipeWireConnection::createVirtualOutput(QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (impl_ == nullptr || impl_->context == nullptr || impl_->loop == nullptr || !impl_->started
        || impl_->stopping) {
        if (error != nullptr) {
            *error = QStringLiteral("PipeWire is not connected");
        }
        return false;
    }

    pw_thread_loop_lock(impl_->loop);
    if (impl_->virtualOutputModule != nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        return true;
    }

    const QByteArray arguments = pipeWireVirtualOutputModuleArguments();
    errno = 0;
    impl_->virtualOutputModule = pw_context_load_module(
        impl_->context,
        "libpipewire-module-loopback",
        arguments.constData(),
        nullptr);
    if (impl_->virtualOutputModule != nullptr) {
        pw_impl_module_add_listener(
            impl_->virtualOutputModule,
            &impl_->virtualOutputModuleListener,
            &Impl::kModuleEvents,
            impl_);
    }
    const int savedError = errno;
    const bool created = impl_->virtualOutputModule != nullptr;
    pw_thread_loop_unlock(impl_->loop);

    if (!created) {
        const QString detail = savedError != 0
            ? QString::fromLocal8Bit(std::strerror(savedError))
            : QStringLiteral("module unavailable");
        if (error != nullptr) {
            *error = QStringLiteral("Unable to load PipeWire loopback module: %1").arg(detail);
        }
        qCWarning(auralisAudio) << "PipeWire VirtualOutputCreateFailed" << detail;
        return false;
    }

    qCInfo(auralisAudio) << "PipeWire VirtualOutputCreateRequested persistence=runtime";
    return true;
}

void PipeWireConnection::destroyVirtualOutput()
{
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return;
    }
    pw_thread_loop_lock(impl_->loop);
    pw_impl_module* module = impl_->virtualOutputModule;
    impl_->virtualOutputModule = nullptr;
    if (module != nullptr) {
        spa_hook_remove(&impl_->virtualOutputModuleListener);
        pw_impl_module_destroy(module);
    }
    pw_thread_loop_unlock(impl_->loop);
    if (module != nullptr) {
        qCInfo(auralisAudio) << "PipeWire VirtualOutputDestroyed persistence=runtime";
    }
}

bool PipeWireConnection::ownsVirtualOutput() const noexcept
{
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return false;
    }
    pw_thread_loop_lock(impl_->loop);
    const bool owned = impl_->virtualOutputModule != nullptr;
    pw_thread_loop_unlock(impl_->loop);
    return owned;
}

std::optional<LinkCreateResult> PipeWireConnection::createLink(
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
    bound->propsWritable = false;
    bound->ownershipToken = impl_->nextOwnershipToken++;
    pw_proxy_add_listener(bound->proxy, &bound->proxyListener, &Impl::kProxyEvents, bound);
    impl_->addObjectListener(bound);
    impl_->pendingCreated.push_back(bound);
    impl_->ownedByToken.insert(bound->ownershipToken, bound);
    const uint32_t boundId = pw_proxy_get_bound_id(proxy);
    if (boundId != SPA_ID_INVALID && boundId != 0) {
        bound->globalId = boundId;
        impl_->pendingCreated.removeAll(bound);
        impl_->proxies.insert(boundId, bound);
        impl_->createdLinkIds.insert(boundId);
        impl_->tokenToGlobalId.insert(bound->ownershipToken, boundId);
    }
    const LinkCreateResult result{bound->ownershipToken, bound->globalId};
    pw_thread_loop_unlock(impl_->loop);

    qCInfo(auralisAudio) << "PipeWire LinkCreateRequested route=" << routeId << "out=" << outputNode << ":" << outputPort
                         << "in=" << inputNode << ":" << inputPort << "token=" << result.ownershipToken
                         << "id=" << result.globalId;
    return result;
}

bool PipeWireConnection::destroyOwnedLink(quint64 ownershipToken)
{
    if (ownershipToken == 0) {
        return true;
    }
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return false;
    }
    pw_thread_loop_lock(impl_->loop);
    Impl::BoundProxy* bound = impl_->ownedByToken.value(ownershipToken);
    if (bound == nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        return true;
    }
    const quint32 globalId = bound->globalId;
    if (globalId != 0) {
        impl_->destroyProxy(globalId);
    } else {
        impl_->destroyPending(bound);
    }
    pw_thread_loop_unlock(impl_->loop);
    qCInfo(auralisAudio) << "PipeWire LinkDestroyed token=" << ownershipToken << "id=" << globalId;
    return true;
}

bool PipeWireConnection::destroyForeignLink(quint32 globalId)
{
    if (globalId == 0) {
        return true;
    }
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return false;
    }
    pw_thread_loop_lock(impl_->loop);
    bool owned = false;
    for (auto it = impl_->tokenToGlobalId.constBegin(); it != impl_->tokenToGlobalId.constEnd(); ++it) {
        if (it.value() == globalId) {
            owned = true;
            break;
        }
    }
    if (owned) {
        pw_thread_loop_unlock(impl_->loop);
        return false;
    }
    Impl::BoundProxy* bound = impl_->proxies.value(globalId);
    if (bound != nullptr && bound->proxy != nullptr) {
        impl_->destroyProxy(globalId);
        pw_thread_loop_unlock(impl_->loop);
        qCInfo(auralisAudio) << "PipeWire ForeignLinkDestroyed id=" << globalId;
        return true;
    }
    if (impl_->registry != nullptr) {
        pw_proxy* proxy = static_cast<pw_proxy*>(pw_registry_bind(
            impl_->registry,
            globalId,
            PW_TYPE_INTERFACE_Link,
            PW_VERSION_LINK,
            sizeof(Impl::BoundProxy)));
        if (proxy != nullptr) {
            pw_proxy_destroy(proxy);
            pw_thread_loop_unlock(impl_->loop);
            qCInfo(auralisAudio) << "PipeWire ForeignLinkDestroyed id=" << globalId << "via=bind";
            return true;
        }
    }
    pw_thread_loop_unlock(impl_->loop);
    qCWarning(auralisAudio) << "PipeWire ForeignLinkDestroyFailed id=" << globalId;
    return false;
}

quint32 PipeWireConnection::ownedLinkGlobalId(quint64 ownershipToken) const
{
    if (impl_ == nullptr || impl_->loop == nullptr || ownershipToken == 0) {
        return 0;
    }
    pw_thread_loop_lock(impl_->loop);
    const quint32 globalId = impl_->tokenToGlobalId.value(ownershipToken, 0);
    pw_thread_loop_unlock(impl_->loop);
    return globalId;
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
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return false;
    }
    pw_thread_loop_lock(impl_->loop);
    const Impl::BoundProxy* bound = impl_->proxies.value(nodeId);
    const bool supported =
        bound != nullptr && bound->kind == PipeWireObjectKind::Node && bound->propsWritable;
    pw_thread_loop_unlock(impl_->loop);
    return supported;
}

bool PipeWireConnection::setNodeDelaySeconds(quint32 nodeId, double delaySeconds)
{
    if (impl_ == nullptr) {
        return false;
    }
    const float clamped = static_cast<float>(std::clamp(delaySeconds, 0.0, 0.5));
    return impl_->applyNodeProps(nodeId, std::nullopt, std::nullopt, clamped);
}

bool PipeWireConnection::ensureLatencyCompensatedFanout(const QStringList& sinkNodeNames)
{
    QStringList names;
    for (const QString& name : sinkNodeNames) {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty() && !names.contains(trimmed)) {
            names.push_back(trimmed);
        }
    }
    names.sort();
    if (names.size() < 2 || impl_ == nullptr || impl_->context == nullptr || impl_->loop == nullptr
        || !impl_->started || impl_->stopping) {
        return false;
    }

    pw_thread_loop_lock(impl_->loop);
    if (impl_->fanoutModule != nullptr && impl_->fanoutSinkNames == names) {
        pw_thread_loop_unlock(impl_->loop);
        return true;
    }
    impl_->destroyFanoutLocked();
    const QByteArray arguments = pipeWireSessionFanoutModuleArguments(names);
    errno = 0;
    impl_->fanoutModule = pw_context_load_module(
        impl_->context, "libpipewire-module-combine-stream", arguments.constData(), nullptr);
    if (impl_->fanoutModule != nullptr) {
        impl_->fanoutSinkNames = names;
        pw_impl_module_add_listener(
            impl_->fanoutModule, &impl_->fanoutModuleListener, &Impl::kFanoutModuleEvents, impl_);
    }
    const bool created = impl_->fanoutModule != nullptr;
    pw_thread_loop_unlock(impl_->loop);
    if (created) {
        qCInfo(auralisAudio) << "PipeWire SessionFanoutCreateRequested sinks=" << names;
    } else {
        qCWarning(auralisAudio) << "PipeWire SessionFanoutCreateFailed";
    }
    return created;
}

void PipeWireConnection::destroyLatencyCompensatedFanout()
{
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return;
    }
    pw_thread_loop_lock(impl_->loop);
    const bool had = impl_->fanoutModule != nullptr;
    impl_->destroyFanoutLocked();
    pw_thread_loop_unlock(impl_->loop);
    if (had) {
        qCInfo(auralisAudio) << "PipeWire SessionFanoutDestroyed";
    }
}

bool PipeWireConnection::ensureDelayBridge(
    const QString& endpointId,
    const QString& destNodeName,
    double delaySeconds)
{
    const QString id = endpointId.trimmed();
    if (id.isEmpty() || id.contains(QLatin1Char('"')) || impl_ == nullptr || impl_->context == nullptr
        || impl_->loop == nullptr || !impl_->started || impl_->stopping) {
        return false;
    }
    const double clamped = std::clamp(std::isfinite(delaySeconds) ? delaySeconds : 0.0, 0.0, 0.5);
    if (clamped < 0.0005) {
        destroyDelayBridge(id);
        return true;
    }

    pw_thread_loop_lock(impl_->loop);
    if (Impl::DelayBridge* existing = impl_->delayBridges.value(id)) {
        if (existing->module != nullptr && existing->destNodeName == destNodeName
            && std::abs(existing->delaySec - clamped) < 0.0005) {
            pw_thread_loop_unlock(impl_->loop);
            return true;
        }
    }
    impl_->destroyDelayBridgeLocked(id);
    auto* bridge = new Impl::DelayBridge;
    bridge->impl = impl_;
    bridge->endpointId = id;
    bridge->destNodeName = destNodeName;
    bridge->delaySec = clamped;
    const QByteArray arguments = pipeWireDelayBridgeModuleArguments(id, destNodeName, clamped);
    errno = 0;
    bridge->module = pw_context_load_module(
        impl_->context, "libpipewire-module-loopback", arguments.constData(), nullptr);
    if (bridge->module == nullptr) {
        delete bridge;
        pw_thread_loop_unlock(impl_->loop);
        qCWarning(auralisAudio) << "PipeWire DelayBridgeCreateFailed endpoint=" << id;
        return false;
    }
    pw_impl_module_add_listener(bridge->module, &bridge->listener, &Impl::kDelayModuleEvents, bridge);
    impl_->delayBridges.insert(id, bridge);
    pw_thread_loop_unlock(impl_->loop);
    qCInfo(auralisAudio) << "PipeWire DelayBridgeCreateRequested endpoint=" << id << "delaySec=" << clamped;
    return true;
}

void PipeWireConnection::destroyDelayBridge(const QString& endpointId)
{
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return;
    }
    pw_thread_loop_lock(impl_->loop);
    impl_->destroyDelayBridgeLocked(endpointId.trimmed());
    pw_thread_loop_unlock(impl_->loop);
}

void PipeWireConnection::destroyAllDelayBridges()
{
    if (impl_ == nullptr || impl_->loop == nullptr) {
        return;
    }
    pw_thread_loop_lock(impl_->loop);
    impl_->destroyAllDelayBridgesLocked();
    pw_thread_loop_unlock(impl_->loop);
}

bool PipeWireConnection::setDefaultAudioSink(const QString& nodeName)
{
    const QString name = nodeName.trimmed();
    if (impl_ == nullptr || impl_->loop == nullptr || !impl_->started || impl_->stopping || name.isEmpty()) {
        return false;
    }
    if (name.contains(QLatin1Char('"')) || name.contains(QLatin1Char('\\'))) {
        return false;
    }
    const QByteArray json = "{\"name\":\"" + name.toUtf8() + "\"}";
    pw_thread_loop_lock(impl_->loop);
    Impl::BoundProxy* bound = impl_->proxies.value(impl_->defaultMetadataId);
    if (bound == nullptr || bound->kind != PipeWireObjectKind::Metadata || bound->proxy == nullptr) {
        pw_thread_loop_unlock(impl_->loop);
        return false;
    }
    auto* metadata = reinterpret_cast<pw_metadata*>(bound->proxy);
    const int current = pw_metadata_set_property(
        metadata, PW_ID_CORE, "default.audio.sink", "Spa:String:JSON", json.constData());
    const int configured = pw_metadata_set_property(
        metadata, PW_ID_CORE, "default.configured.audio.sink", "Spa:String:JSON", json.constData());
    pw_thread_loop_unlock(impl_->loop);
    return current >= 0 && configured >= 0;
}

void PipeWireConnection::setLinkErrorHandler(LinkErrorHandler handler)
{
    linkErrorHandler_ = handler;
    if (impl_ != nullptr) {
        impl_->linkErrorHandler = std::move(handler);
    }
}

} // namespace auralis::audio
