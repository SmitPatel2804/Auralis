#include <auralis/bluetooth/DeviceRegistry.h>
#include <auralis/session/SessionListModel.h>
#include <auralis/session/SessionManager.h>

namespace auralis::session {
namespace {

QString deviceDisplayName(SessionManager* manager, const QString& deviceId)
{
    if (manager == nullptr) {
        return deviceId;
    }
    auralis::bluetooth::DeviceRegistry* registry = manager->deviceRegistry();
    if (registry == nullptr) {
        return deviceId;
    }
    const auralis::bluetooth::BluetoothDeviceData* device = registry->findByObjectPath(deviceId);
    if (device == nullptr) {
        return deviceId;
    }
    return device->displayName();
}

} // namespace

QString userFacingSessionState(SessionState state)
{
    switch (state) {
    case SessionState::Idle:
        return QStringLiteral("Inactive");
    case SessionState::Starting:
        return QStringLiteral("Starting…");
    case SessionState::Active:
        return QStringLiteral("Active");
    case SessionState::Degraded:
        return QStringLiteral("Degraded");
    case SessionState::Recovering:
        return QStringLiteral("Recovering…");
    case SessionState::Stopping:
        return QStringLiteral("Stopping…");
    case SessionState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

SessionListModel::SessionListModel(SessionManager* manager, QObject* parent)
    : QAbstractListModel(parent)
    , manager_(manager)
{
    if (manager_ == nullptr) {
        return;
    }
    connect(manager_, &SessionManager::sessionAdded, this, &SessionListModel::onAdded);
    connect(manager_, &SessionManager::sessionRemoved, this, &SessionListModel::onRemoved);
    connect(manager_, &SessionManager::sessionUpdated, this, &SessionListModel::onUpdated);
    connect(manager_, &SessionManager::currentSessionIdChanged, this, [this]() {
        if (!ids_.isEmpty()) {
            emit dataChanged(index(0, 0), index(ids_.size() - 1, 0), {ActiveRole});
        }
    });
    reload();
}

int SessionListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ids_.size();
}

QVariant SessionListModel::data(const QModelIndex& index, int role) const
{
    if (manager_ == nullptr || !index.isValid() || index.row() < 0 || index.row() >= ids_.size()) {
        return {};
    }
    const std::optional<AuralisSession> session = manager_->sessionById(ids_.at(index.row()));
    if (!session.has_value()) {
        return {};
    }
    int connected = 0;
    int enabled = 0;
    for (const SessionDevice& device : session->devices) {
        if (device.enabled) {
            ++enabled;
            if (device.runtime.connected) {
                ++connected;
            }
        }
    }
    const bool active = manager_->currentSessionId() == session->id
        && (session->state == SessionState::Active || session->state == SessionState::Degraded
            || session->state == SessionState::Recovering || session->state == SessionState::Starting
            || session->state == SessionState::Stopping);
    switch (role) {
    case IdRole:
        return session->id;
    case NameRole:
    case Qt::DisplayRole:
        return session->name;
    case StateRole:
        return static_cast<int>(session->state);
    case StateTextRole:
        return toString(session->state);
    case StateLabelRole:
        return userFacingSessionState(session->state);
    case SourceIdRole:
        return session->sourceId;
    case DeviceCountRole:
        return session->devices.size();
    case ConnectedDeviceCountRole:
        return connected;
    case EnabledDeviceCountRole:
        return enabled;
    case ActiveRole:
        return active;
    case DegradedRole:
        return session->state == SessionState::Degraded;
    case GroupVolumeRole:
        return session->groupVolume;
    case MutedRole:
        return session->muted;
    case LastUsedRole:
        return session->lastUsedAt.isValid() ? session->lastUsedAt.toLocalTime().toString(Qt::ISODate) : QString();
    case RecoveryPolicyRole:
        return toString(session->recoveryPolicy);
    case AutoReconnectRole:
        return session->autoReconnect;
    case ErrorTextRole:
        return session->error.detail;
    default:
        return {};
    }
}

QHash<int, QByteArray> SessionListModel::roleNames() const
{
    return {
        {IdRole, "sessionId"},
        {NameRole, "name"},
        {StateRole, "state"},
        {StateTextRole, "stateText"},
        {StateLabelRole, "stateLabel"},
        {SourceIdRole, "sourceId"},
        {DeviceCountRole, "deviceCount"},
        {ConnectedDeviceCountRole, "connectedDeviceCount"},
        {EnabledDeviceCountRole, "enabledDeviceCount"},
        {ActiveRole, "active"},
        {DegradedRole, "degraded"},
        {GroupVolumeRole, "groupVolume"},
        {MutedRole, "muted"},
        {LastUsedRole, "lastUsed"},
        {RecoveryPolicyRole, "recoveryPolicy"},
        {AutoReconnectRole, "autoReconnect"},
        {ErrorTextRole, "errorText"},
    };
}

void SessionListModel::reload()
{
    beginResetModel();
    ids_.clear();
    if (manager_ != nullptr) {
        for (const AuralisSession& session : manager_->sessions()) {
            ids_.push_back(session.id);
        }
    }
    endResetModel();
}

int SessionListModel::indexOf(const QString& sessionId) const
{
    return ids_.indexOf(sessionId);
}

void SessionListModel::onAdded(const QString& sessionId)
{
    if (ids_.contains(sessionId)) {
        return;
    }
    const int row = ids_.size();
    beginInsertRows(QModelIndex(), row, row);
    ids_.push_back(sessionId);
    endInsertRows();
}

void SessionListModel::onRemoved(const QString& sessionId)
{
    const int row = indexOf(sessionId);
    if (row < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    ids_.removeAt(row);
    endRemoveRows();
}

void SessionListModel::onUpdated(const QString& sessionId)
{
    const int row = indexOf(sessionId);
    if (row < 0) {
        return;
    }
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}

SessionMemberListModel::SessionMemberListModel(SessionManager* manager, QObject* parent)
    : QAbstractListModel(parent)
    , manager_(manager)
{
    if (manager_ != nullptr) {
        connect(manager_, &SessionManager::sessionUpdated, this, &SessionMemberListModel::onUpdated);
        connect(manager_, &SessionManager::sessionRemoved, this, [this](const QString& id) {
            if (id == sessionId_) {
                setSessionId({});
            }
        });
    }
}

QString SessionMemberListModel::sessionId() const
{
    return sessionId_;
}

void SessionMemberListModel::setSessionId(const QString& sessionId)
{
    if (sessionId_ == sessionId) {
        reload();
        return;
    }
    sessionId_ = sessionId;
    reload();
    emit sessionIdChanged();
}

int SessionMemberListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return members_.size();
}

QVariant SessionMemberListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= members_.size()) {
        return {};
    }
    const SessionDevice& device = members_.at(index.row());
    switch (role) {
    case DeviceIdRole:
        return device.deviceId;
    case DisplayNameRole:
    case Qt::DisplayRole:
        return deviceDisplayName(manager_, device.deviceId);
    case RoleNameRole:
        return toString(device.role);
    case EnabledRole:
        return device.enabled;
    case ConnectedRole:
        return device.runtime.connected;
    case EndpointAvailableRole:
        return device.runtime.endpointAvailable;
    case RouteActiveRole:
        return device.runtime.routeActive;
    case RecoveringRole:
        return device.runtime.recovering;
    case VolumeRole:
        return device.volumeTrim;
    case MutedRole:
        return device.muted;
    case ErrorTextRole:
        return device.runtime.lastError.detail;
    default:
        return {};
    }
}

QHash<int, QByteArray> SessionMemberListModel::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {DisplayNameRole, "displayName"},
        {RoleNameRole, "roleName"},
        {EnabledRole, "enabled"},
        {ConnectedRole, "connected"},
        {EndpointAvailableRole, "endpointAvailable"},
        {RouteActiveRole, "routeActive"},
        {RecoveringRole, "recovering"},
        {VolumeRole, "volume"},
        {MutedRole, "muted"},
        {ErrorTextRole, "errorText"},
    };
}

void SessionMemberListModel::reload()
{
    beginResetModel();
    members_.clear();
    if (manager_ != nullptr && !sessionId_.isEmpty()) {
        const std::optional<AuralisSession> session = manager_->sessionById(sessionId_);
        if (session.has_value()) {
            members_ = session->devices;
        }
    }
    endResetModel();
}

void SessionMemberListModel::onUpdated(const QString& sessionId)
{
    if (sessionId == sessionId_) {
        reload();
    }
}

} // namespace auralis::session
