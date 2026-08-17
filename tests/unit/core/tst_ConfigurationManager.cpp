#include <auralis/core/ConfigurationManager.h>

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class TstConfigurationManager : public QObject {
    Q_OBJECT

private:
    static std::unique_ptr<QSettings> makeIsolatedSettings(const QTemporaryDir& dir)
    {
        const QString path = dir.filePath(QStringLiteral("auralis-test.ini"));
        return std::make_unique<QSettings>(path, QSettings::IniFormat);
    }

    void clearOverrides()
    {
        qunsetenv("AURALIS_LOG_FILE_ENABLED");
        qunsetenv("AURALIS_LOG_FILE_PATH");
        qunsetenv("AURALIS_UI_SHOW_DEVELOPER_STATUS");
    }

private slots:
    void init()
    {
        clearOverrides();
    }

    void cleanup()
    {
        clearOverrides();
    }

    void defaultsWithoutHardwareOrExternalFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        QVERIFY(manager.isInitialized());
        QCOMPARE(manager.applicationName(), QStringLiteral("Auralis"));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(manager.logFilePath().isEmpty());
        QCOMPARE(manager.showDeveloperStatus(), true);
    }

    void environmentOverrideTakesPrecedence()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto settings = makeIsolatedSettings(dir);
        settings->setValue(QStringLiteral("logging/fileEnabled"), false);
        settings->sync();

        qputenv("AURALIS_LOG_FILE_ENABLED", "true");
        qputenv("AURALIS_LOG_FILE_PATH", "/tmp/auralis-test.log");
        qputenv("AURALIS_UI_SHOW_DEVELOPER_STATUS", "0");

        auralis::core::ConfigurationManager manager(std::move(settings));
        QVERIFY(manager.initialize());
        QCOMPARE(manager.fileLoggingEnabled(), true);
        QCOMPARE(manager.logFilePath(), QStringLiteral("/tmp/auralis-test.log"));
        QCOMPARE(manager.showDeveloperStatus(), false);
    }

    void invalidBooleanOverrideFallsBackSafely()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        qputenv("AURALIS_LOG_FILE_ENABLED", "maybe");
        qputenv("AURALIS_UI_SHOW_DEVELOPER_STATUS", "definitely");

        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QCOMPARE(manager.showDeveloperStatus(), true);
    }

    void multipleInstancesDoNotCorruptIsolatedSettings()
    {
        QTemporaryDir dirA;
        QTemporaryDir dirB;
        QVERIFY(dirA.isValid());
        QVERIFY(dirB.isValid());

        auto settingsA = makeIsolatedSettings(dirA);
        settingsA->setValue(QStringLiteral("application/name"), QStringLiteral("Alpha"));
        settingsA->sync();

        auto settingsB = makeIsolatedSettings(dirB);
        settingsB->setValue(QStringLiteral("application/name"), QStringLiteral("Beta"));
        settingsB->sync();

        auralis::core::ConfigurationManager managerA(std::move(settingsA));
        auralis::core::ConfigurationManager managerB(std::move(settingsB));
        QVERIFY(managerA.initialize());
        QVERIFY(managerB.initialize());
        QCOMPARE(managerA.applicationName(), QStringLiteral("Alpha"));
        QCOMPARE(managerB.applicationName(), QStringLiteral("Beta"));
    }
};

QTEST_GUILESS_MAIN(TstConfigurationManager)
#include "tst_ConfigurationManager.moc"
