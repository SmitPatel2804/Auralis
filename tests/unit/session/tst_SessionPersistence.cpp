#include <auralis/session/SessionPersistence.h>

#include <QTemporaryDir>
#include <QtTest>

using auralis::session::AuralisSession;
using auralis::session::SessionDevice;
using auralis::session::SessionDeviceRole;
using auralis::session::SessionPersistence;
using auralis::session::SessionPersistenceDocument;
using auralis::session::SessionState;

class TstSessionPersistence : public QObject {
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir dir;
        SessionPersistence persistence(dir.filePath(QStringLiteral("sessions.json")));
        SessionPersistenceDocument document;
        AuralisSession session;
        session.id = QStringLiteral("abc");
        session.name = QStringLiteral("Living Room");
        session.sourceId = QStringLiteral("src:7:Stream/Output/Audio");
        SessionDevice left;
        left.deviceId = QStringLiteral("AA:BB:CC:DD:EE:01");
        left.role = SessionDeviceRole::Left;
        left.volumeTrim = 0.9;
        session.devices.push_back(left);
        session.groupVolume = 0.7;
        session.state = SessionState::Active;
        document.sessions.push_back(session);

        QVERIFY(persistence.save(document));
        const SessionPersistenceDocument loaded = persistence.load();
        QCOMPARE(loaded.sessions.size(), 1);
        QCOMPARE(loaded.sessions.front().id, session.id);
        QCOMPARE(loaded.sessions.front().devices.size(), 1);
        QCOMPARE(loaded.sessions.front().devices.front().deviceId, left.deviceId);
        QCOMPARE(loaded.sessions.front().groupVolume, 0.7);
        QVERIFY(loaded.sessions.front().state == SessionState::Idle);
    }

    void invalidJsonDoesNotCrash()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sessions.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{not json");
        file.close();

        SessionPersistence persistence(path);
        const SessionPersistenceDocument loaded = persistence.load();
        QVERIFY(loaded.sessions.isEmpty());
    }

    void skipsDuplicateDeviceIds()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sessions.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({
  "schemaVersion": 1,
  "sessions": [{
    "id": "abc",
    "name": "Dup",
    "devices": [
      {"deviceId": "AA:BB:CC:DD:EE:01", "role": "Left", "enabled": true, "volume": 1, "muted": false},
      {"deviceId": "AA:BB:CC:DD:EE:01", "role": "Right", "enabled": true, "volume": 0.5, "muted": false}
    ]
  }]
})");
        file.close();

        SessionPersistence persistence(path);
        const SessionPersistenceDocument loaded = persistence.load();
        QCOMPARE(loaded.sessions.size(), 1);
        QCOMPARE(loaded.sessions.front().devices.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TstSessionPersistence)
#include "tst_SessionPersistence.moc"
