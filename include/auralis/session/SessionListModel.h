#pragma once

#include <auralis/session/AuralisSession.h>
#include <auralis/session/SessionTypes.h>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>

namespace auralis::session {

class SessionManager;

class SessionListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        StateRole,
        StateTextRole,
        StateLabelRole,
        SourceIdRole,
        DeviceCountRole,
        ConnectedDeviceCountRole,
        EnabledDeviceCountRole,
        ActiveRole,
        DegradedRole,
        GroupVolumeRole,
        MutedRole,
        LastUsedRole,
        RecoveryPolicyRole,
        AutoReconnectRole,
        ErrorTextRole
    };

    explicit SessionListModel(SessionManager* manager, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QString sessionIdAt(int row) const;

    void reload();

private:
    int indexOf(const QString& sessionId) const;
    void onAdded(const QString& sessionId);
    void onRemoved(const QString& sessionId);
    void onUpdated(const QString& sessionId);

    QPointer<SessionManager> manager_;
    QStringList ids_;
};

class SessionMemberListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString sessionId READ sessionId WRITE setSessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        DeviceIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        RoleNameRole,
        EnabledRole,
        ConnectedRole,
        EndpointAvailableRole,
        RouteActiveRole,
        RecoveringRole,
        VolumeRole,
        MutedRole,
        ErrorTextRole
    };

    explicit SessionMemberListModel(SessionManager* manager, QObject* parent = nullptr);

    QString sessionId() const;
    void setSessionId(const QString& sessionId);
    int count() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reload();

signals:
    void sessionIdChanged();
    void countChanged();

private:
    void onUpdated(const QString& sessionId);

    QPointer<SessionManager> manager_;
    QString sessionId_;
    QVector<SessionDevice> members_;
};

QString userFacingSessionState(SessionState state);

} // namespace auralis::session
