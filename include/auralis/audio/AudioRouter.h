#pragma once

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioSource.h>
#include <auralis/audio/IPipeWireLinkBackend.h>
#include <auralis/audio/LinkManager.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/RoutePlanner.h>
#include <auralis/audio/VolumeController.h>

#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <cstdint>
#include <optional>

namespace auralis::audio {

class AudioSourceListModel;

class AudioRouter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sources READ sources CONSTANT)
    Q_PROPERTY(int sourceCount READ sourceCount NOTIFY sourcesChanged)
    Q_PROPERTY(QString currentRouteId READ currentRouteId NOTIFY routeChanged)
    Q_PROPERTY(QString routeStateText READ routeStateText NOTIFY routeStateChanged)
    Q_PROPERTY(QString lastErrorText READ lastErrorText NOTIFY routeError)
    Q_PROPERTY(bool routeEnabled READ routeEnabled NOTIFY routeStateChanged)
    Q_PROPERTY(double routeVolume READ routeVolume NOTIFY routeChanged)
    Q_PROPERTY(bool routeMuted READ routeMuted NOTIFY routeChanged)
    Q_PROPERTY(bool volumeCapable READ volumeCapable NOTIFY routeChanged)
    Q_PROPERTY(int ownedLinkCount READ ownedLinkCount NOTIFY routeChanged)

public:
    AudioRouter(
        PipeWireObjectStore* store,
        AudioEndpointRegistry* endpoints,
        IPipeWireLinkBackend* backend,
        QObject* parent = nullptr);

    QAbstractItemModel* sources() const;
    int sourceCount() const;
    QString currentRouteId() const;
    QString routeStateText() const;
    QString lastErrorText() const;
    bool routeEnabled() const;
    double routeVolume() const;
    bool routeMuted() const;
    bool volumeCapable() const;
    int ownedLinkCount() const;

    QVector<AudioSource> sourceList() const;
    QVector<AudioRoute> routes() const;
    std::optional<AudioRoute> routeById(const QString& id) const;
    AudioRoute* mutableRoute(const QString& id);

    Q_INVOKABLE QString createRoute(const QString& sourceId, const QStringList& destinationEndpointIds);
    Q_INVOKABLE void removeRoute(const QString& routeId);
    Q_INVOKABLE void activateRoute(const QString& routeId);
    Q_INVOKABLE void deactivateRoute(const QString& routeId);
    Q_INVOKABLE void setRouteSource(const QString& routeId, const QString& sourceId);
    Q_INVOKABLE void setRouteDestinations(const QString& routeId, const QStringList& destinationEndpointIds);
    Q_INVOKABLE void setDestinationVolume(const QString& endpointId, double value);
    Q_INVOKABLE void setDestinationMuted(const QString& endpointId, bool muted);
    Q_INVOKABLE void setRouteVolume(const QString& routeId, double value);
    Q_INVOKABLE void setRouteMuted(const QString& routeId, bool muted);

    void refreshSources();
    void handleGraphChanged();
    void handleConnectionState(PipeWireConnectionState state, bool initialSyncComplete);
    void shutdown();

signals:
    void sourcesChanged();
    void routeAdded(const QString& routeId);
    void routeChanged(const QString& routeId);
    void routeRemoved(const QString& routeId);
    void routeStateChanged(const QString& routeId, RouteState state);
    void routeError(const QString& routeId, RouteError error, const QString& detail);

private:
    void setState(AudioRoute& route, RouteState state);
    void setError(AudioRoute& route, RouteError category, const QString& detail);
    void emitRouteSignals(const AudioRoute& route);
    bool validateSelection(const QString& sourceId, const QStringList& destinationIds, RouteErrorInfo* error) const;
    void beginActivation(AudioRoute& route, quint64 generation);
    void finishActivationIfReady(AudioRoute& route);
    void rollback(AudioRoute& route, RouteError category, const QString& detail, bool disable);
    void replanIfEnabled(AudioRoute& route);
    bool linksOperational(const AudioRoute& route) const;
    bool linksHaveError(const AudioRoute& route) const;
    void bindPendingLinkIds(AudioRoute& route);
    QVector<QPair<QString, quint32>> destinationNodes(const AudioRoute& route) const;

    PipeWireObjectStore* store_ = nullptr;
    AudioEndpointRegistry* endpoints_ = nullptr;
    IPipeWireLinkBackend* backend_ = nullptr;
    RoutePlanner planner_;
    LinkManager links_;
    VolumeController volume_;
    AudioSourceListModel* sourceModel_ = nullptr;
    QVector<AudioSource> sources_;
    QVector<AudioRoute> routes_;
    QHash<QString, quint64> generations_;
    QTimer activationTimer_;
    PipeWireConnectionState connectionState_ = PipeWireConnectionState::Stopped;
    bool initialSyncComplete_ = false;
    quint64 nextGeneration_ = 1;
};

} // namespace auralis::audio
