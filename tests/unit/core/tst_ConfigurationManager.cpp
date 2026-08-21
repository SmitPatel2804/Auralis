#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/Logger.h>

#include <QSettings>
#include <QSignalSpy>
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
        QCOMPARE(manager.fileLoggingEnabled(), true);
        QVERIFY(manager.logFilePath().contains(QStringLiteral("logs"), Qt::CaseInsensitive));
        QVERIFY(manager.logFilePath().endsWith(QStringLiteral(".log")));
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
        QCOMPARE(manager.fileLoggingEnabled(), true);
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

    void settersPersist()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        QVERIFY(manager.setShowDeveloperStatus(false));
        QCOMPARE(manager.showDeveloperStatus(), false);
        QVERIFY(manager.setRestoreLastSession(true));
        QCOMPARE(manager.restoreLastSession(), true);
        QVERIFY(manager.resetToDefaults());
        QCOMPARE(manager.restoreLastSession(), false);
        QCOMPARE(manager.showDeveloperStatus(), true);
    }

    void legacyPathWithoutLoggingPreferenceMigratesToExecutionLog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto settings = makeIsolatedSettings(dir);
        const QString legacyPath = dir.filePath(QStringLiteral("legacy-fixed.log"));
        settings->setValue(QStringLiteral("logging/filePath"), legacyPath);
        settings->sync();

        auralis::core::ConfigurationManager manager(std::move(settings));
        QVERIFY(manager.initialize());
        QVERIFY(manager.fileLoggingEnabled());
        QVERIFY(manager.logFilePath() != legacyPath);
        QVERIFY(manager.logFilePath().contains(QStringLiteral("logs"), Qt::CaseInsensitive));
        QVERIFY(manager.logFilePath().contains(QStringLiteral("auralis-")));
        QVERIFY(manager.logFilePath().endsWith(QStringLiteral(".log")));
    }

    void explicitLoggingPreferencePreservesCustomPath()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto settings = makeIsolatedSettings(dir);
        const QString customPath = dir.filePath(QStringLiteral("custom.log"));
        settings->setValue(QStringLiteral("logging/fileEnabled"), false);
        settings->setValue(QStringLiteral("logging/filePath"), customPath);
        settings->sync();

        auralis::core::ConfigurationManager manager(std::move(settings));
        QVERIFY(manager.initialize());
        QVERIFY(!manager.fileLoggingEnabled());
        QCOMPARE(manager.logFilePath(), customPath);
    }

    void settingCustomPathMarksLoggingPreferenceExplicit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString customPath = dir.filePath(QStringLiteral("chosen.log"));

        {
            auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
            QVERIFY(manager.initialize());
            QVERIFY(manager.setLogFilePath(customPath));
        }

        auralis::core::ConfigurationManager reloaded(makeIsolatedSettings(dir));
        QVERIFY(reloaded.initialize());
        QVERIFY(reloaded.fileLoggingEnabled());
        QCOMPARE(reloaded.logFilePath(), customPath);
    }

    void navigationAndWindowGeometryAreClampedAndPersisted()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto settings = makeIsolatedSettings(dir);
        settings->setValue(QStringLiteral("ui/lastNavPage"), -99);
        settings->setValue(QStringLiteral("ui/windowWidth"), 100);
        settings->setValue(QStringLiteral("ui/windowHeight"), 100);
        settings->sync();

        auralis::core::ConfigurationManager manager(std::move(settings));
        QVERIFY(manager.initialize());
        QCOMPARE(manager.lastNavPage(), 0);
        QCOMPARE(manager.windowWidth(), 880);
        QCOMPARE(manager.windowHeight(), 600);

        QVERIFY(manager.setLastNavPage(99));
        QVERIFY(manager.setWindowWidth(1));
        QVERIFY(manager.setWindowHeight(1));
        QCOMPARE(manager.lastNavPage(), 5);
        QCOMPARE(manager.windowWidth(), 880);
        QCOMPARE(manager.windowHeight(), 600);

        auralis::core::ConfigurationManager persisted(makeIsolatedSettings(dir));
        QVERIFY(persisted.initialize());
        QCOMPARE(persisted.lastNavPage(), 5);
        QCOMPARE(persisted.windowWidth(), 880);
        QCOMPARE(persisted.windowHeight(), 600);
    }

    void fileLoggingToggleAppliesToLogger()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        const QString path = dir.filePath(QStringLiteral("auralis.log"));
        QVERIFY(manager.setLogFilePath(path));
        QVERIFY(manager.setFileLoggingEnabled(true));
        QCOMPARE(manager.fileLoggingEnabled(), true);
        QVERIFY(auralis::core::Logger::isFileLoggingActive());
        QVERIFY(manager.lastErrorText().isEmpty());
        QVERIFY(manager.setFileLoggingEnabled(false));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void fileLoggingEnableEmptyPathFailsWithoutStateDivergence()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        QVERIFY(manager.setFileLoggingEnabled(false));
        QVERIFY(manager.setLogFilePath(QString()));

        QSignalSpy enabledSpy(&manager, &auralis::core::ConfigurationManager::fileLoggingEnabledChanged);
        QVERIFY(!manager.setFileLoggingEnabled(true));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
        QVERIFY(!manager.lastErrorText().trimmed().isEmpty());
        QCOMPARE(enabledSpy.count(), 0);

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void fileLoggingEnableInvalidPathFailsWithoutStateDivergence()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        QVERIFY(manager.setFileLoggingEnabled(false));
        const QString impossible = dir.filePath(QStringLiteral("missing-parent/auralis.log"));
        QVERIFY(manager.setLogFilePath(impossible));

        QSignalSpy enabledSpy(&manager, &auralis::core::ConfigurationManager::fileLoggingEnabledChanged);
        QVERIFY(!manager.setFileLoggingEnabled(true));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
        QVERIFY(!manager.lastErrorText().trimmed().isEmpty());
        QCOMPARE(enabledSpy.count(), 0);

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void fileLoggingEnableValidPathActivatesRuntimeLogger()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        const QString path = dir.filePath(QStringLiteral("auralis-valid.log"));

        QSignalSpy enabledSpy(&manager, &auralis::core::ConfigurationManager::fileLoggingEnabledChanged);
        QVERIFY(manager.setLogFilePath(path));
        QVERIFY(manager.setFileLoggingEnabled(true));
        QCOMPARE(manager.fileLoggingEnabled(), true);
        QVERIFY(auralis::core::Logger::isFileLoggingActive());
        QVERIFY(manager.lastErrorText().isEmpty());
        QCOMPARE(enabledSpy.count(), 0);

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void fileLoggingDisableDeactivatesRuntimeLogger()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        const QString path = dir.filePath(QStringLiteral("auralis-disable.log"));
        QVERIFY(manager.setLogFilePath(path));
        QVERIFY(manager.setFileLoggingEnabled(true));
        QVERIFY(manager.setFileLoggingEnabled(false));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
        QVERIFY(manager.lastErrorText().isEmpty());
        QVERIFY(manager.setFileLoggingEnabled(false));
        QCOMPARE(manager.fileLoggingEnabled(), false);

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void fileLoggingRepeatedEnableIsIdempotent()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        const QString path = dir.filePath(QStringLiteral("auralis-idempotent.log"));
        QVERIFY(manager.setLogFilePath(path));

        QSignalSpy enabledSpy(&manager, &auralis::core::ConfigurationManager::fileLoggingEnabledChanged);
        QVERIFY(manager.setFileLoggingEnabled(true));
        QCOMPARE(enabledSpy.count(), 0);
        QVERIFY(manager.setFileLoggingEnabled(true));
        QCOMPARE(enabledSpy.count(), 0);
        QCOMPARE(manager.fileLoggingEnabled(), true);
        QVERIFY(auralis::core::Logger::isFileLoggingActive());

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }

    void reconcileFileLoggingActivationFailureClearsEnabledState()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        auralis::core::Logger::shutdown();
        QVERIFY(auralis::core::Logger::initialize());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auralis::core::ConfigurationManager manager(makeIsolatedSettings(dir));
        QVERIFY(manager.initialize());
        const QString path = dir.filePath(QStringLiteral("auralis-reconcile.log"));
        QVERIFY(manager.setLogFilePath(path));
        QVERIFY(manager.setFileLoggingEnabled(true));

        manager.reconcileFileLoggingActivationFailure(QStringLiteral("Unable to enable file logging at the selected path."));
        QCOMPARE(manager.fileLoggingEnabled(), false);
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
        QVERIFY(!manager.lastErrorText().isEmpty());

        auralis::core::Logger::shutdown();
#else
        QSKIP("File logging is compiled out");
#endif
    }
};

QTEST_GUILESS_MAIN(TstConfigurationManager)
#include "tst_ConfigurationManager.moc"
