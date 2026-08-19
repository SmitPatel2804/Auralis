#pragma once

#include <QObject>
#include <QString>

namespace auralis::session {

class SessionManager;

class SelectedSessionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sessionId READ sessionId WRITE setSessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY stateLabelChanged)
    Q_PROPERTY(QString sourceId READ sourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceNameChanged)
    Q_PROPERTY(double groupVolume READ groupVolume NOTIFY groupVolumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(QString recoveryPolicy READ recoveryPolicy NOTIFY recoveryPolicyChanged)
    Q_PROPERTY(bool exists READ exists NOTIFY existsChanged)

public:
    explicit SelectedSessionViewModel(SessionManager* manager, QObject* parent = nullptr);

    QString sessionId() const;
    void setSessionId(const QString& sessionId);

    QString name() const;
    QString stateLabel() const;
    QString sourceId() const;
    QString sourceName() const;
    double groupVolume() const;
    bool muted() const;
    QString recoveryPolicy() const;
    bool exists() const;

signals:
    void sessionIdChanged();
    void nameChanged();
    void stateLabelChanged();
    void sourceIdChanged();
    void sourceNameChanged();
    void groupVolumeChanged();
    void mutedChanged();
    void recoveryPolicyChanged();
    void existsChanged();

private:
    void refresh();
    void handleSessionUpdated(const QString& sessionId);
    void handleSessionRemoved(const QString& sessionId);

    SessionManager* manager_ = nullptr;
    QString sessionId_;
    QString name_;
    QString stateLabel_;
    QString sourceId_;
    QString sourceName_;
    double groupVolume_ = 1.0;
    bool muted_ = false;
    QString recoveryPolicy_;
    bool exists_ = false;
};

} // namespace auralis::session
