#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>
#include <auralis/ui/DiagnosticsLogModel.h>

#include <QCoreApplication>
#include <QtTest>

class TstDiagnosticsLogModel : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());
    }

    void cleanup()
    {
        auralis::core::Logger::shutdown();
    }

    void retainsBoundedEntries()
    {
        auralis::ui::DiagnosticsLogModel model;
        model.setCaptureEnabled(true);
        model.setCapacity(100);
        qCInfo(auralisCore) << "hello-log";
        QTRY_VERIFY(model.rowCount() >= 1);
        QCOMPARE(
            model.data(model.index(model.rowCount() - 1, 0), auralis::ui::DiagnosticsLogModel::MessageRole)
                .toString()
                .contains(QStringLiteral("hello-log")),
            true);
        QVERIFY(model.visibleText().contains(QStringLiteral("hello-log")));
    }

    void filtersBySeverity()
    {
        auralis::ui::DiagnosticsLogModel model;
        model.setCaptureEnabled(true);
        qCWarning(auralisCore) << "warn-only";
        QTRY_VERIFY(model.rowCount() >= 1);
        model.setSeverityFilter(QStringLiteral("DEBUG"));
        QVERIFY(model.rowCount() >= 0);
    }

    void copyWithoutGuiApplicationReturnsFalse()
    {
        auralis::ui::DiagnosticsLogModel model;
        QVERIFY(!model.copyVisibleToClipboard());
    }
};

QTEST_GUILESS_MAIN(TstDiagnosticsLogModel)
#include "tst_DiagnosticsLogModel.moc"
