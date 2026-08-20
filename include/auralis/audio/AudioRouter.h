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
#include <QDateTime>
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
class RouteListModel;

class AudioRouter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sources READ sources CONSTANT)
    Q_PROPERTY(QAbstractItemModel* routes READ routeModel CONSTANT)
    Q_PROPERTY(int sourceCount READ sourceCount NOTIFY sourcesChanged)
    Q_PROPERTY(QString currentRouteId READ currentRouteId NOTIFY currentRouteIdChanged)
    Q_PROPERTY(QString routeStateText READ routeStateText NOTIFY routeStateTextChanged)
    Q_PROPERTY(QString lastErrorText READ lastErrorText NOTIFY lastErrorTextChanged)
    Q_PROPERTY(bool routeEnabled READ routeEnabled NOTIFY routeEnabledChanged)
    Q_PROPERTY(double routeVolume READ routeVolume NOTIFY routeVolumeChanged)
    Q_PROPERTY(bool routeMuted READ routeMuted NOTIFY routeMutedChanged)
    Q_PROPERTY(bool volumeCapable READ volumeCapable NOTIFY volumeCapableChanged)
    Q_PROPERTY(int ownedLinkCount READ ownedLinkCount NOTIFY ownedLinkCountChanged)

public:
    AudioRouter(
        PipeWireObjectStore* store,
        AudioEndpointRegistry* endpoints,
        IPipeWireLinkBackend* backend,
        QObject* parent = nullptr);
    ~AudioRouter() override;

    QAbstractItemModel* sources() const;
    QAbstractItemModel* routeModel() const;
    int sourceCount() const;
    QString currentRouteId() const;
    QString routeStateText() const;
    QString lastErrorText() const;
    bool routeEnabled() const;
    double routeVolume() const;
    bool routeMuted() const;
    bool volumeCapable() const;
    int ownedLinkCount() const;

    void setActivationTimeoutMs(int milliseconds);
    int activationTimeoutMs() const noexcept;

    QVector<AudioSource> sourceList() const;
    QVector<AudioRoute> routes() const;
    std::optional<AudioRoute> routeById(const QString& id) const;
    AudioRoute* mutableRoute(const QString& id);

    Q_INVOKABLE QString createRoute(const QString& sourceId, const QStringList& destinationEndpointIds);
    QString createSessionRoute(
        const QString& sessionId,
        const QString& sourceId,
        const QStringList& destinationEndpointIds);
    Q_INVOKABLE void removeRoute(const QString& routeId);
    Q_INVOKABLE void activateRoute(const QString& routeId);
    Q_INVOKABLE void deactivateRoute(const QString& routeId);
    Q_INVOKABLE void setRouteSource(const QString& routeId, const QString& sourceId);
    Q_INVOKABLE void setRouteDestinations(const QString& routeId, const QStringList& destinationEndpointIds);
    Q_INVOKABLE void setDestinationVolume(const QString& endpointId, double value);
    Q_INVOKABLE void setDestinationMuted(const QString& endpointId, bool muted);
    Q_INVOKABLE void setRouteVolume(const QString& routeId, double value);
    Q_INVOKABLE void setRouteMuted(const QString& routeId, bool muted);
    Q_INVOKABLE QString sourceDisplayName(const QString& sourceId) const;

    void refreshSources();
    void handleGraphChanged();
    void handleConnectionState(PipeWireConnectionState state, bool initialSyncComplete);
    void handleOwnedLinkError(quint64 ownershipToken, const QString& detail);
    void shutdown();

signals:
    void sourcesChanged();
    void routeAdded(const QString& routeId);
    void routeChanged(const QString& routeId);
    void routeRemoved(const QString& routeId);
    void routeStateChanged(const QString& routeId, RouteState state);
    void routeError(const QString& routeId, RouteError error, const QString& detail);
    void currentRouteIdChanged();
    void routeStateTextChanged();
    void lastErrorTextChanged();
    void routeEnabledChanged();
    void routeVolumeChanged();
    void routeMutedChanged();
    void volumeCapableChanged();
    void ownedLinkCountChanged();

private:
    void setState(AudioRoute& route, RouteState state);
    void setError(AudioRoute& route, RouteError category, const QString& detail);
    void emitRouteSignals(const AudioRoute& route);
    void emitQmlPropertyNotifications();
    bool validateSelection(const QString& sourceId, const QStringList& destinationIds, RouteErrorInfo* error) const;
    void beginActivation(AudioRoute& route, quint64 generation);
    void clearConflictingLinks(const ResolvedRoutePlan& plan);
    QVector<OwnedLink> tryAdoptExistingLinks(const QString& routeId, const ResolvedRoutePlan& plan);
    void finishActivationIfReady(AudioRoute& route);
    void rollback(AudioRoute& route, RouteError category, const QString& detail, bool disable);
    void replanIfEnabled(AudioRoute& route);
    bool linksOperational(const AudioRoute& route) const;
    bool linksHaveError(const AudioRoute& route) const;
    void refreshOwnedLinkIds(AudioRoute& route);
    void armActivationTimeout(const QString& routeId, quint64 generation);
    void stopActivationTimeout(const QString& routeId);
    void stopAllActivationTimeouts();
    void invalidateOwnedLinks(AudioRoute& route);
    QVector<QPair<QString, quint32>> destinationNodes(const AudioRoute& route) const;
    const AudioRoute* plannerRoute() const;
    QString createRouteInternal(
        const QString& sourceId,
        const QStringList& destinationEndpointIds,
        RouteOwnerType ownerType,
        const QString& ownerId);
    bool rejectPlannerMutation(const AudioRoute& route);

    PipeWireObjectStore* store_ = nullptr;
    AudioEndpointRegistry* endpoints_ = nullptr;
    IPipeWireLinkBackend* backend_ = nullptr;
    RoutePlanner planner_;
    LinkManager links_;
    VolumeController volume_;
    AudioSourceListModel* sourceModel_ = nullptr;
    RouteListModel* routeModel_ = nullptr;
    QVector<AudioSource> sources_;
    QVector<AudioRoute> routes_;
    QHash<QString, quint64> generations_;
    QHash<QString, qint64> lastActivateAttemptMs_;
    QHash<QString, QTimer*> activationTimers_;
    QHash<QString, QVector<OwnedLink>> replanBackups_;
    int activationTimeoutMs_ = 5000;
    PipeWireConnectionState connectionState_ = PipeWireConnectionState::Stopped;
    bool initialSyncComplete_ = false;
    quint64 nextGeneration_ = 1;
    QString qmlCurrentRouteId_;
    QString qmlRouteStateText_;
    QString qmlLastErrorText_;
    bool qmlRouteEnabled_ = false;
    double qmlRouteVolume_ = 1.0;
    bool qmlRouteMuted_ = false;
    bool qmlVolumeCapable_ = false;
    int qmlOwnedLinkCount_ = 0;
};

} // namespace auralis::audio
