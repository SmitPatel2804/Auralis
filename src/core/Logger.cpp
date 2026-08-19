#include <auralis/core/Logger.h>

#include <auralis/core/LoggingCategories.h>

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace auralis::core {
namespace {

struct LoggerState {
    QMutex mutex;
    bool initialized = false;
    bool consoleEnabled = true;
    bool fileEnabled = false;
    std::unique_ptr<QFile> file;
    QtMessageHandler previousHandler = nullptr;
    Logger::Observer observer;
};

LoggerState& loggerState()
{
    static LoggerState state;
    return state;
}

const char* severityName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARNING";
    case QtCriticalMsg:
        return "CRITICAL";
    case QtFatalMsg:
        return "FATAL";
    }

    return "INFO";
}

QString categoryLabel(const QMessageLogContext& context)
{
    const QString category = QString::fromUtf8(context.category ? context.category : "default");
    constexpr QStringView prefix = u"auralis.";
    if (category.startsWith(prefix)) {
        return category.sliced(prefix.size());
    }
    return category;
}

QString formatLine(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    return QStringLiteral("%1 %2 %3 %4")
        .arg(timestamp, QString::fromLatin1(severityName(type)), categoryLabel(context), message);
}

void writeLine(const QString& line)
{
    LoggerState& state = loggerState();
    const QByteArray utf8 = line.toUtf8();

    if (state.consoleEnabled) {
        std::fprintf(stderr, "%s\n", utf8.constData());
        std::fflush(stderr);
    }

    if (state.file && state.file->isOpen()) {
        state.file->write(utf8);
        state.file->write("\n", 1);
        state.file->flush();
    }
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    LoggerState& state = loggerState();
    Logger::Observer observer;
    QString category;
    {
        QMutexLocker locker(&state.mutex);
        writeLine(formatLine(type, context, message));
        observer = state.observer;
        category = categoryLabel(context);
    }

    if (observer) {
        observer(type, category, message);
    }

    if (type == QtFatalMsg) {
        std::abort();
    }
}

bool openFileSinkLocked(const QString& filePath)
{
    LoggerState& state = loggerState();
    state.file.reset();
    state.fileEnabled = false;

    if (filePath.isEmpty()) {
        return false;
    }

    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }

    state.file = std::move(file);
    state.fileEnabled = true;
    return true;
}

} // namespace

bool Logger::initialize()
{
    return initialize(Options{});
}

bool Logger::initialize(const Options& options)
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);

    if (state.initialized) {
        return true;
    }

    state.consoleEnabled = options.enableConsole;
    state.previousHandler = qInstallMessageHandler(&messageHandler);
    state.initialized = true;

    bool fileOk = true;
    if (options.enableFile) {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        fileOk = openFileSinkLocked(options.filePath);
#else
        fileOk = false;
#endif
    }

    locker.unlock();

    qCInfo(auralisCore) << "Auralis logger initialized";
    if (options.enableFile && !fileOk) {
        qCWarning(auralisCore) << "File logging requested but the log file could not be opened:"
                               << options.filePath;
    }

    return true;
}

void Logger::shutdown()
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);

    if (!state.initialized) {
        return;
    }

    if (state.file) {
        state.file->flush();
        state.file->close();
        state.file.reset();
    }

    state.fileEnabled = false;
    state.initialized = false;
    state.observer = {};
    qInstallMessageHandler(state.previousHandler);
    state.previousHandler = nullptr;
}

bool Logger::isInitialized()
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);
    return state.initialized;
}

bool Logger::isFileLoggingActive()
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);
    return state.fileEnabled && state.file && state.file->isOpen();
}

bool Logger::enableFileLogging(const QString& filePath)
{
#ifndef AURALIS_ENABLE_FILE_LOGGING
    Q_UNUSED(filePath)
    return false;
#else
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);

    if (!state.initialized) {
        return false;
    }

    if (!openFileSinkLocked(filePath)) {
        locker.unlock();
        qCWarning(auralisCore) << "File logging could not be enabled:" << filePath;
        return false;
    }

    return true;
#endif
}

void Logger::setObserver(Observer observer)
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);
    state.observer = std::move(observer);
}

void Logger::disableFileLogging()
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);

    if (state.file) {
        state.file->flush();
        state.file->close();
        state.file.reset();
    }
    state.fileEnabled = false;
}

} // namespace auralis::core
