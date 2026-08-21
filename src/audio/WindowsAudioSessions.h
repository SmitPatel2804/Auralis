#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>

namespace auralis::audio {

struct WindowsAudioSession {
    std::uint32_t processId = 0;
    QString applicationName;
    QString displayName;
    QString sessionIdentifier;
    bool active = false;
};

QVector<WindowsAudioSession> enumerateWindowsAudioSessions(QString* errorText = nullptr);
QByteArray windowsAudioSessionFingerprint(const QVector<WindowsAudioSession>& sessions);

} // namespace auralis::audio
