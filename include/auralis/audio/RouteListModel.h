#pragma once

#include <auralis/audio/AudioRoute.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace auralis::audio {

class AudioRouter;

class RouteListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        SourceIdRole,
        SourceNameRole,
        DestinationCountRole,
        DestinationIdsRole,
        ActiveRole,
        EnabledRole,
        StateRole,
        StateTextRole,
        ErrorTextRole,
        VolumeRole,
        MutedRole
    };

    explicit RouteListModel(AudioRouter* router, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reload();

private:
    int indexOf(const QString& routeId) const;
    void onAdded(const QString& routeId);
    void onRemoved(const QString& routeId);
    void onChanged(const QString& routeId);

    QPointer<AudioRouter> router_;
    QStringList ids_;
};

} // namespace auralis::audio
