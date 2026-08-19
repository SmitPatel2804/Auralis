#include <auralis/ui/NotificationController.h>

#include <QtTest>

class TstNotificationController : public QObject {
    Q_OBJECT

private slots:
    void postsAndDismisses()
    {
        auralis::ui::NotificationController notes;
        QCOMPARE(notes.count(), 0);
        notes.postError(QStringLiteral("Bluetooth"), QStringLiteral("adapter down"));
        QCOMPARE(notes.count(), 1);
        QCOMPARE(notes.warningCount(), 1);
        QVERIFY(notes.latestErrorText().contains(QStringLiteral("adapter down")));
        const QString id = notes.data(notes.index(0, 0), auralis::ui::NotificationController::IdRole).toString();
        notes.dismiss(id);
        QCOMPARE(notes.count(), 0);
    }

    void infoIsNotWarning()
    {
        auralis::ui::NotificationController notes;
        notes.postInfo(QStringLiteral("Nav"), QStringLiteral("ok"));
        QCOMPARE(notes.warningCount(), 0);
        notes.clearNonSticky();
        QCOMPARE(notes.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TstNotificationController)
#include "tst_NotificationController.moc"
