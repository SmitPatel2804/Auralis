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
    QString filePath;
    qint64 maxFileBytes = 5 * 1024 * 1024;
    int retainRotatedFiles = 3;
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

void rotateFilesLocked()
{
    LoggerState& state = loggerState();
    if (state.filePath.isEmpty() || state.retainRotatedFiles <= 0) {
        return;
    }

    if (state.file) {
        state.file->flush();
        state.file->close();
        state.file.reset();
    }

    for (int i = state.retainRotatedFiles - 1; i >= 1; --i) {
        const QString from = QStringLiteral("%1.%2").arg(state.filePath).arg(i);
        const QString to = QStringLiteral("%1.%2").arg(state.filePath).arg(i + 1);
        if (QFile::exists(to)) {
            QFile::remove(to);
        }
        if (QFile::exists(from)) {
            QFile::rename(from, to);
        }
    }
    const QString first = QStringLiteral("%1.1").arg(state.filePath);
    if (QFile::exists(first)) {
        QFile::remove(first);
    }
    if (QFile::exists(state.filePath)) {
        QFile::rename(state.filePath, first);
    }
}

bool openFileSinkLocked(const QString& filePath)
{
    LoggerState& state = loggerState();
    state.file.reset();
    state.fileEnabled = false;
    state.filePath = filePath;

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

void maybeRotateLocked()
{
    LoggerState& state = loggerState();
    if (!state.file || !state.file->isOpen() || state.maxFileBytes <= 0) {
        return;
    }
    if (state.file->size() < state.maxFileBytes) {
        return;
    }
    rotateFilesLocked();
    openFileSinkLocked(state.filePath);
}

bool requiresImmediateFlush(QtMsgType type)
{
    return type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg;
}

void writeLine(QtMsgType type, const QString& line)
{
    LoggerState& state = loggerState();
    const QByteArray utf8 = line.toUtf8();
    const bool flushNow = requiresImmediateFlush(type);

    if (state.consoleEnabled) {
        std::fprintf(stderr, "%s\n", utf8.constData());
        if (flushNow) {
            std::fflush(stderr);
        }
    }

    if (state.file && state.file->isOpen()) {
        maybeRotateLocked();
        if (state.file && state.file->isOpen()) {
            state.file->write(utf8);
            state.file->write("\n", 1);
            // Normal records remain in the OS/file buffer and are flushed on
            // shutdown or rotation. Warnings and failures are forced out
            // immediately so crash diagnostics remain durable.
            if (flushNow) {
                state.file->flush();
            }
        }
    }
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    LoggerState& state = loggerState();
    Logger::Observer observer;
    QString category;
    {
        QMutexLocker locker(&state.mutex);
        writeLine(type, formatLine(type, context, message));
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
    state.maxFileBytes = options.maxFileBytes;
    state.retainRotatedFiles = options.retainRotatedFiles;
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
    state.filePath.clear();
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

QString Logger::activeFilePath()
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);
    return state.fileEnabled && state.file && state.file->isOpen() ? state.filePath : QString();
}

void Logger::setRotationPolicy(qint64 maxFileBytes, int retainRotatedFiles)
{
    LoggerState& state = loggerState();
    QMutexLocker locker(&state.mutex);
    state.maxFileBytes = maxFileBytes > 0 ? maxFileBytes : state.maxFileBytes;
    state.retainRotatedFiles = retainRotatedFiles > 0 ? retainRotatedFiles : state.retainRotatedFiles;
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
    state.filePath.clear();
}

} // namespace auralis::core
