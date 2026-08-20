#include <auralis/session/SessionListModel.h>
#include <auralis/session/SessionManager.h>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TstSessionListModel : public QObject {
    Q_OBJECT

private slots:
    void insertUpdateRemove()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::session::SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("s.json")));
        QVERIFY(manager.initialize());
        auto* model = qobject_cast<QAbstractItemModel*>(manager.sessionList());
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 0);

        QSignalSpy inserted(model, &QAbstractItemModel::rowsInserted);
        const QString id = manager.createSession(QStringLiteral("Living Room"));
        QVERIFY(!id.isEmpty());
        QCOMPARE(model->rowCount(), 1);
        QVERIFY(inserted.count() >= 1);
        QCOMPARE(model->data(model->index(0, 0), auralis::session::SessionListModel::NameRole).toString(),
                 QStringLiteral("Living Room"));
        QCOMPARE(model->data(model->index(0, 0), auralis::session::SessionListModel::IdRole).toString(), id);

        QVERIFY(manager.renameSession(id, QStringLiteral("Kitchen")) == auralis::session::SessionCommandResult::Accepted);
        QCOMPARE(model->data(model->index(0, 0), auralis::session::SessionListModel::NameRole).toString(),
                 QStringLiteral("Kitchen"));

        QSignalSpy removed(model, &QAbstractItemModel::rowsRemoved);
        QVERIFY(manager.deleteSession(id) == auralis::session::SessionCommandResult::Accepted);
        QCOMPARE(model->rowCount(), 0);
        QVERIFY(removed.count() >= 1);
    }

    void memberCountNotifiesWhenDeviceAdded()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::session::SessionManager manager(nullptr, nullptr, dir.filePath(QStringLiteral("s.json")));
        QVERIFY(manager.initialize());
        const QString id = manager.createSession(QStringLiteral("Members"));
        QVERIFY(!id.isEmpty());

        auto* members = qobject_cast<auralis::session::SessionMemberListModel*>(manager.sessionMembers());
        QVERIFY(members != nullptr);
        members->setSessionId(id);
        QCOMPARE(members->count(), 0);

        QSignalSpy countSpy(members, &auralis::session::SessionMemberListModel::countChanged);
        QVERIFY(manager.addDevice(id, QStringLiteral("AA:BB:CC:DD:EE:FF"))
                == auralis::session::SessionCommandResult::Accepted);
        QCOMPARE(members->count(), 1);
        QVERIFY(countSpy.count() >= 1);
        QCOMPARE(qobject_cast<auralis::session::SessionListModel*>(manager.sessionList())->sessionIdAt(0), id);
    }
};

QTEST_GUILESS_MAIN(TstSessionListModel)
#include "tst_SessionListModel.moc"
