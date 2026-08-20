#include <auralis/session/SelectedSessionViewModel.h>

#include <auralis/session/AuralisSession.h>
#include <auralis/session/SessionListModel.h>
#include <auralis/session/SessionManager.h>
#include <auralis/session/SessionTypes.h>

#include <optional>

namespace auralis::session {

SelectedSessionViewModel::SelectedSessionViewModel(SessionManager* manager, QObject* parent)
    : QObject(parent)
    , manager_(manager)
    , recoveryPolicy_(toString(RecoveryPolicy::ReconnectAndRestore))
{
    if (manager_ == nullptr) {
        return;
    }
    connect(manager_, &SessionManager::sessionUpdated, this, &SelectedSessionViewModel::handleSessionUpdated);
    connect(manager_, &SessionManager::sessionRemoved, this, &SelectedSessionViewModel::handleSessionRemoved);
    connect(manager_, &SessionManager::sessionsChanged, this, &SelectedSessionViewModel::refresh);
    connect(manager_, &SessionManager::sessionStateChanged, this, [this](const QString& sessionId, SessionState, SessionState) {
        if (sessionId == sessionId_) {
            refresh();
        }
    });
}

QString SelectedSessionViewModel::sessionId() const
{
    return sessionId_;
}

void SelectedSessionViewModel::setSessionId(const QString& sessionId)
{
    if (sessionId_ == sessionId) {
        refresh();
        return;
    }
    sessionId_ = sessionId;
    emit sessionIdChanged();
    refresh();
}

QString SelectedSessionViewModel::name() const
{
    return name_;
}

QString SelectedSessionViewModel::stateLabel() const
{
    return stateLabel_;
}

QString SelectedSessionViewModel::sourceId() const
{
    return sourceId_;
}

QString SelectedSessionViewModel::sourceName() const
{
    return sourceName_;
}

double SelectedSessionViewModel::groupVolume() const
{
    return groupVolume_;
}

bool SelectedSessionViewModel::muted() const
{
    return muted_;
}

QString SelectedSessionViewModel::recoveryPolicy() const
{
    return recoveryPolicy_;
}

bool SelectedSessionViewModel::exists() const
{
    return exists_;
}

void SelectedSessionViewModel::handleSessionUpdated(const QString& sessionId)
{
    if (sessionId == sessionId_) {
        refresh();
    }
}

void SelectedSessionViewModel::handleSessionRemoved(const QString& sessionId)
{
    if (sessionId == sessionId_) {
        refresh();
    }
}

void SelectedSessionViewModel::refresh()
{
    const std::optional<AuralisSession> session =
        manager_ != nullptr ? manager_->sessionById(sessionId_) : std::nullopt;
    const bool exists = session.has_value() && !sessionId_.isEmpty();
    const QString name = exists ? session->name : QString();
    const QString stateLabel = exists ? userFacingSessionState(session->state) : QString();
    const QString sourceId = exists ? session->sourceId : QString();
    const QString sourceName = manager_ != nullptr ? manager_->sourceDisplayName(sourceId) : sourceId;
    const double groupVolume = exists ? session->groupVolume : 1.0;
    const bool muted = exists && session->muted;
    const QString recoveryPolicy = exists ? toString(session->recoveryPolicy) : toString(RecoveryPolicy::ReconnectAndRestore);

    if (exists_ != exists) {
        exists_ = exists;
        emit existsChanged();
    }
    if (name_ != name) {
        name_ = name;
        emit nameChanged();
    }
    if (stateLabel_ != stateLabel) {
        stateLabel_ = stateLabel;
        emit stateLabelChanged();
    }
    if (sourceId_ != sourceId) {
        sourceId_ = sourceId;
        emit sourceIdChanged();
    }
    if (sourceName_ != sourceName) {
        sourceName_ = sourceName;
        emit sourceNameChanged();
    }
    if (groupVolume_ != groupVolume) {
        groupVolume_ = groupVolume;
        emit groupVolumeChanged();
    }
    if (muted_ != muted) {
        muted_ = muted;
        emit mutedChanged();
    }
    if (recoveryPolicy_ != recoveryPolicy) {
        recoveryPolicy_ = recoveryPolicy;
        emit recoveryPolicyChanged();
    }
}

} // namespace auralis::session
