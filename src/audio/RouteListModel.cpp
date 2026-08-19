#include <auralis/audio/RouteListModel.h>

#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/AudioSource.h>

#include <optional>

namespace auralis::audio {

RouteListModel::RouteListModel(AudioRouter* router, QObject* parent)
    : QAbstractListModel(parent)
    , router_(router)
{
    if (router_ == nullptr) {
        return;
    }
    connect(router_, &AudioRouter::routeAdded, this, &RouteListModel::onAdded);
    connect(router_, &AudioRouter::routeRemoved, this, &RouteListModel::onRemoved);
    connect(router_, &AudioRouter::routeChanged, this, &RouteListModel::onChanged);
    reload();
}

int RouteListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ids_.size();
}

QVariant RouteListModel::data(const QModelIndex& index, int role) const
{
    if (router_ == nullptr || !index.isValid() || index.row() < 0 || index.row() >= ids_.size()) {
        return {};
    }
    const std::optional<AudioRoute> route = router_->routeById(ids_.at(index.row()));
    if (!route.has_value()) {
        return {};
    }
    switch (role) {
    case IdRole:
        return route->id;
    case SourceIdRole:
        return route->sourceId;
    case SourceNameRole:
        return router_->sourceDisplayName(route->sourceId);
    case DestinationCountRole:
        return route->destinationIds.size();
    case DestinationIdsRole:
        return route->destinationIds;
    case ActiveRole:
        return route->state == RouteState::Active || route->state == RouteState::Degraded;
    case EnabledRole:
        return route->enabled;
    case StateRole:
        return static_cast<int>(route->state);
    case StateTextRole:
        return toString(route->state);
    case ErrorTextRole:
        return route->error.detail;
    case VolumeRole:
        return route->volume;
    case MutedRole:
        return route->muted;
    case Qt::DisplayRole:
        return route->id;
    default:
        return {};
    }
}

QHash<int, QByteArray> RouteListModel::roleNames() const
{
    return {
        {IdRole, "routeId"},
        {SourceIdRole, "sourceId"},
        {SourceNameRole, "sourceName"},
        {DestinationCountRole, "destinationCount"},
        {DestinationIdsRole, "destinationIds"},
        {ActiveRole, "active"},
        {EnabledRole, "enabled"},
        {StateRole, "state"},
        {StateTextRole, "stateText"},
        {ErrorTextRole, "errorText"},
        {VolumeRole, "volume"},
        {MutedRole, "muted"},
    };
}

void RouteListModel::reload()
{
    beginResetModel();
    ids_.clear();
    if (router_ != nullptr) {
        for (const AudioRoute& route : router_->routes()) {
            ids_.push_back(route.id);
        }
    }
    endResetModel();
}

int RouteListModel::indexOf(const QString& routeId) const
{
    return ids_.indexOf(routeId);
}

void RouteListModel::onAdded(const QString& routeId)
{
    if (ids_.contains(routeId)) {
        return;
    }
    const int row = ids_.size();
    beginInsertRows(QModelIndex(), row, row);
    ids_.push_back(routeId);
    endInsertRows();
}

void RouteListModel::onRemoved(const QString& routeId)
{
    const int row = indexOf(routeId);
    if (row < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    ids_.removeAt(row);
    endRemoveRows();
}

void RouteListModel::onChanged(const QString& routeId)
{
    const int row = indexOf(routeId);
    if (row < 0) {
        return;
    }
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}

} // namespace auralis::audio
