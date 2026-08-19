#include <auralis/session/SessionPersistence.h>

#include <QFile>
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
        QString error;
        const SessionPersistenceDocument loaded = persistence.load(&error);
        QVERIFY(loaded.sessions.isEmpty());
        QVERIFY(!error.isEmpty());
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

    void createsMissingParentDirectory()
    {
        QTemporaryDir dir;
        SessionPersistence persistence(dir.filePath(QStringLiteral("does-not-exist/nested/sessions.json")));
        SessionPersistenceDocument document;
        AuralisSession session;
        session.id = QStringLiteral("abc");
        session.name = QStringLiteral("Nested");
        document.sessions.push_back(session);
        QString error;
        QVERIFY(persistence.save(document, &error));
        QVERIFY(error.isEmpty());
        const SessionPersistenceDocument loaded = persistence.load();
        QCOMPARE(loaded.sessions.size(), 1);
        QCOMPARE(loaded.sessions.front().id, session.id);
    }

    void saveFailsWhenParentIsAFile()
    {
        QTemporaryDir dir;
        const QString blocker = dir.filePath(QStringLiteral("blocker"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SessionPersistence persistence(blocker + QStringLiteral("/nested/sessions.json"));
        SessionPersistenceDocument document;
        AuralisSession session;
        session.id = QStringLiteral("abc");
        session.name = QStringLiteral("Fail");
        document.sessions.push_back(session);
        QString error;
        QVERIFY(!persistence.save(document, &error));
        QVERIFY(!error.isEmpty());
    }

    void unsupportedSchemaVersionFailsLoad()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sessions.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({
  "schemaVersion": 99,
  "sessions": [{
    "id": "abc",
    "name": "Future",
    "devices": []
  }]
})");
        file.close();

        SessionPersistence persistence(path);
        QString error;
        const SessionPersistenceDocument loaded = persistence.load(&error);
        QVERIFY(loaded.sessions.isEmpty());
        QVERIFY(error.contains(QStringLiteral("Unsupported session schema version")));
    }

    void skipsDuplicateSessionIds()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sessions.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({
  "schemaVersion": 1,
  "sessions": [
    {"id": "abc", "name": "First", "devices": []},
    {"id": "abc", "name": "Second", "devices": []}
  ]
})");
        file.close();

        SessionPersistence persistence(path);
        const SessionPersistenceDocument loaded = persistence.load();
        QCOMPARE(loaded.sessions.size(), 1);
        QCOMPARE(loaded.sessions.front().id, QStringLiteral("abc"));
        QCOMPARE(loaded.sessions.front().name, QStringLiteral("First"));
    }
};

QTEST_GUILESS_MAIN(TstSessionPersistence)
#include "tst_SessionPersistence.moc"
