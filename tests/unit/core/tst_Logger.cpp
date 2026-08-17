#include <auralis/core/Logger.h>

#include <QTemporaryDir>
#include <QtTest>

class TstLogger : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        auralis::core::Logger::shutdown();
    }

    void cleanup()
    {
        auralis::core::Logger::shutdown();
    }

    void initializesAndIsIdempotent()
    {
        QVERIFY(auralis::core::Logger::initialize());
        QVERIFY(auralis::core::Logger::isInitialized());
        QVERIFY(auralis::core::Logger::initialize());
        QVERIFY(auralis::core::Logger::isInitialized());
    }

    void shutdownResetsState()
    {
        QVERIFY(auralis::core::Logger::initialize());
        auralis::core::Logger::shutdown();
        QVERIFY(!auralis::core::Logger::isInitialized());
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
    }

    void invalidFilePathDoesNotTerminate()
    {
        auralis::core::Logger::Options options;
        options.enableConsole = true;
        options.enableFile = true;
        options.filePath = QStringLiteral("/this/path/does/not/exist/auralis-phase1.log");

        QVERIFY(auralis::core::Logger::initialize(options));
        QVERIFY(auralis::core::Logger::isInitialized());
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
    }

    void enableFileLoggingFailsGracefully()
    {
        QVERIFY(auralis::core::Logger::initialize());
        QVERIFY(!auralis::core::Logger::enableFileLogging(
            QStringLiteral("/this/path/does/not/exist/auralis-phase1.log")));
        QVERIFY(auralis::core::Logger::isInitialized());
        QVERIFY(!auralis::core::Logger::isFileLoggingActive());
    }

    void validFileLoggingCanBeEnabled()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("auralis.log"));

        QVERIFY(auralis::core::Logger::initialize());
        QVERIFY(auralis::core::Logger::enableFileLogging(path));
        QVERIFY(auralis::core::Logger::isFileLoggingActive());
    }
};

QTEST_GUILESS_MAIN(TstLogger)
#include "tst_Logger.moc"
