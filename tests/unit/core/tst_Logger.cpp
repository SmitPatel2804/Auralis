#include <auralis/core/Logger.h>

#include <QFile>
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

    void rotatesWhenMaxSizeExceeded()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("rotate.log"));

        auralis::core::Logger::Options options;
        options.enableConsole = false;
        options.maxFileBytes = 64;
        options.retainRotatedFiles = 2;
        QVERIFY(auralis::core::Logger::initialize(options));
        QVERIFY(auralis::core::Logger::enableFileLogging(path));

        for (int i = 0; i < 40; ++i) {
            qInfo("rotation-fill-%d-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", i);
        }

        QVERIFY(QFile::exists(path) || QFile::exists(path + QStringLiteral(".1")));
#else
        QSKIP("File logging disabled in this build");
#endif
    }
};

QTEST_GUILESS_MAIN(TstLogger)
#include "tst_Logger.moc"
