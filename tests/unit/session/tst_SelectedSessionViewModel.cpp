#include <auralis/session/SelectedSessionViewModel.h>
#include <auralis/session/SessionManager.h>
#include <auralis/session/SessionTypes.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using auralis::session::SelectedSessionViewModel;
using auralis::session::SessionCommandResult;
using auralis::session::SessionManager;

class TstSelectedSessionViewModel : public QObject {
    Q_OBJECT

private slots:
    void selectedSessionIsAuthoritativeAndIsolated()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("sessions.json")));
        QVERIFY(manager.initialize());

        const QString idA = manager.createSession(QStringLiteral("Alpha"));
        const QString idB = manager.createSession(QStringLiteral("Beta"));
        QVERIFY(!idA.isEmpty());
        QVERIFY(!idB.isEmpty());
        QVERIFY(manager.setSource(idA, QStringLiteral("src-a")) == SessionCommandResult::Accepted);
        QVERIFY(manager.setSource(idB, QStringLiteral("src-b")) == SessionCommandResult::Accepted);
        QVERIFY(manager.setGroupVolume(idA, 0.25) == SessionCommandResult::Accepted);
        QVERIFY(manager.setGroupVolume(idB, 0.75) == SessionCommandResult::Accepted);
        QVERIFY(manager.setSessionMuted(idA, false) == SessionCommandResult::Accepted);
        QVERIFY(manager.setSessionMuted(idB, true) == SessionCommandResult::Accepted);
        QVERIFY(manager.setRecoveryPolicy(idA, QStringLiteral("None")) == SessionCommandResult::Accepted);
        QVERIFY(manager.setRecoveryPolicy(idB, QStringLiteral("RestoreRoutesOnly")) == SessionCommandResult::Accepted);

        auto* selected = qobject_cast<SelectedSessionViewModel*>(manager.selectedSession());
        QVERIFY(selected != nullptr);

        selected->setSessionId(idB);
        QCOMPARE(selected->sessionId(), idB);
        QCOMPARE(selected->name(), QStringLiteral("Beta"));
        QCOMPARE(selected->sourceId(), QStringLiteral("src-b"));
        QCOMPARE(selected->groupVolume(), 0.75);
        QCOMPARE(selected->muted(), true);
        QCOMPARE(selected->recoveryPolicy(), QStringLiteral("RestoreRoutesOnly"));
        QVERIFY(selected->exists());

        selected->setSessionId(idA);
        QCOMPARE(selected->name(), QStringLiteral("Alpha"));
        QCOMPARE(selected->sourceId(), QStringLiteral("src-a"));
        QCOMPARE(selected->groupVolume(), 0.25);
        QCOMPARE(selected->muted(), false);
        QCOMPARE(selected->recoveryPolicy(), QStringLiteral("None"));

        QVERIFY(manager.setGroupVolume(idB, 0.11) == SessionCommandResult::Accepted);
        QVERIFY(manager.setSource(idB, QStringLiteral("src-b-changed")) == SessionCommandResult::Accepted);
        QVERIFY(manager.setSessionMuted(idB, false) == SessionCommandResult::Accepted);
        QVERIFY(manager.setRecoveryPolicy(idB, QStringLiteral("ReconnectAndRestore")) == SessionCommandResult::Accepted);

        QCOMPARE(selected->sessionId(), idA);
        QCOMPARE(selected->sourceId(), QStringLiteral("src-a"));
        QCOMPARE(selected->groupVolume(), 0.25);
        QCOMPARE(selected->muted(), false);
        QCOMPARE(selected->recoveryPolicy(), QStringLiteral("None"));
        QCOMPARE(manager.sessionById(idB)->groupVolume, 0.11);
        QCOMPARE(manager.sessionById(idB)->sourceId, QStringLiteral("src-b-changed"));
    }
};

QTEST_GUILESS_MAIN(TstSelectedSessionViewModel)
#include "tst_SelectedSessionViewModel.moc"
