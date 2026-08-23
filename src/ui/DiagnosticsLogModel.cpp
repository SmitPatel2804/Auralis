#include <auralis/ui/DiagnosticsLogModel.h>

#include <auralis/core/Logger.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QMetaObject>

namespace auralis::ui {
namespace {

QString severityName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARNING");
    case QtCriticalMsg:
        return QStringLiteral("CRITICAL");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

} // namespace

DiagnosticsLogModel::DiagnosticsLogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

DiagnosticsLogModel::~DiagnosticsLogModel()
{
    removeObserver();
}

int DiagnosticsLogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return visibleRows_.size();
}

QVariant DiagnosticsLogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= visibleRows_.size()) {
        return {};
    }
    const Entry& entry = entries_.at(visibleRows_.at(index.row()));
    switch (role) {
    case TimestampRole:
        return entry.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
    case SeverityRole:
        return entry.severity;
    case CategoryRole:
        return entry.category;
    case MessageRole:
        return entry.message;
    case Qt::DisplayRole:
        return entry.message;
    default:
        return {};
    }
}

QHash<int, QByteArray> DiagnosticsLogModel::roleNames() const
{
    return {
        {TimestampRole, "timestamp"},
        {SeverityRole, "severity"},
        {CategoryRole, "category"},
        {MessageRole, "message"},
    };
}

int DiagnosticsLogModel::capacity() const noexcept
{
    return capacity_;
}

void DiagnosticsLogModel::setCapacity(int value)
{
    const int next = value < 100 ? 100 : value;
    if (capacity_ == next) {
        return;
    }
    capacity_ = next;
    beginResetModel();
    trimLocked();
    rebuildVisibleRows();
    endResetModel();
    emit capacityChanged();
    emit filterChanged();
}

QString DiagnosticsLogModel::severityFilter() const
{
    return severityFilter_;
}

void DiagnosticsLogModel::setSeverityFilter(const QString& value)
{
    if (severityFilter_ == value) {
        return;
    }
    beginResetModel();
    severityFilter_ = value;
    rebuildVisibleRows();
    endResetModel();
    emit filterChanged();
}

QString DiagnosticsLogModel::categoryFilter() const
{
    return categoryFilter_;
}

void DiagnosticsLogModel::setCategoryFilter(const QString& value)
{
    if (categoryFilter_ == value) {
        return;
    }
    beginResetModel();
    categoryFilter_ = value;
    rebuildVisibleRows();
    endResetModel();
    emit filterChanged();
}

int DiagnosticsLogModel::visibleCount() const
{
    return visibleRows_.size();
}

bool DiagnosticsLogModel::captureEnabled() const noexcept
{
    return captureEnabled_;
}

void DiagnosticsLogModel::setCaptureEnabled(bool enabled)
{
    if (captureEnabled_ == enabled) {
        return;
    }
    captureEnabled_ = enabled;
    if (enabled) {
        installObserver();
    } else {
        removeObserver();
    }
    emit captureEnabledChanged();
}

QString DiagnosticsLogModel::visibleText() const
{
    QStringList lines;
    lines.reserve(visibleRows_.size());
    for (int row : visibleRows_) {
        const Entry& entry = entries_.at(row);
        lines.push_back(
            entry.timestamp.toString(Qt::ISODateWithMs) + QLatin1Char(' ') + entry.severity + QLatin1Char(' ')
            + entry.category + QLatin1Char(' ') + entry.message);
    }
    return lines.join(QLatin1Char('\n'));
}

bool DiagnosticsLogModel::copyVisibleToClipboard() const
{
    auto* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (gui == nullptr) {
        return false;
    }
    QClipboard* clipboard = gui->clipboard();
    if (clipboard == nullptr) {
        return false;
    }
    clipboard->setText(visibleText());
    return true;
}

void DiagnosticsLogModel::clear()
{
    beginResetModel();
    entries_.clear();
    visibleRows_.clear();
    endResetModel();
    emit filterChanged();
}

void DiagnosticsLogModel::appendFromLogger(QtMsgType type, const QString& category, const QString& message)
{
    if (!captureEnabled_) {
        return;
    }
    if (type == QtDebugMsg && severityFilter_.isEmpty()) {
        return;
    }

    Entry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.severity = severityName(type);
    entry.category = category;
    entry.message = message;

    if (entries_.size() >= capacity_) {
        const bool oldestVisible = !visibleRows_.isEmpty() && visibleRows_.constFirst() == 0;
        if (oldestVisible) {
            beginRemoveRows(QModelIndex(), 0, 0);
        }
        entries_.removeFirst();
        if (oldestVisible) {
            visibleRows_.removeFirst();
        }
        for (int& row : visibleRows_) {
            --row;
        }
        if (oldestVisible) {
            endRemoveRows();
        }
    }

    const int storageIndex = entries_.size();
    if (matches(entry)) {
        const int visibleRow = visibleRows_.size();
        beginInsertRows(QModelIndex(), visibleRow, visibleRow);
        entries_.push_back(entry);
        visibleRows_.push_back(storageIndex);
        endInsertRows();
    } else {
        entries_.push_back(entry);
    }
    emit filterChanged();
}

bool DiagnosticsLogModel::matches(const Entry& entry) const
{
    if (!severityFilter_.isEmpty() && entry.severity.compare(severityFilter_, Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!categoryFilter_.isEmpty() && !entry.category.contains(categoryFilter_, Qt::CaseInsensitive)) {
        return false;
    }
    return true;
}

void DiagnosticsLogModel::rebuildVisibleRows()
{
    visibleRows_.clear();
    visibleRows_.reserve(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        if (matches(entries_.at(i))) {
            visibleRows_.push_back(i);
        }
    }
}

void DiagnosticsLogModel::trimLocked()
{
    while (entries_.size() > capacity_) {
        entries_.removeFirst();
    }
}

void DiagnosticsLogModel::installObserver()
{
    if (observerInstalled_) {
        return;
    }
    auralis::core::Logger::setObserver([this](QtMsgType type, const QString& category, const QString& message) {
        QMetaObject::invokeMethod(
            this,
            [this, type, category, message]() { appendFromLogger(type, category, message); },
            Qt::QueuedConnection);
    });
    observerInstalled_ = true;
}

void DiagnosticsLogModel::removeObserver()
{
    if (!observerInstalled_) {
        return;
    }
    auralis::core::Logger::setObserver({});
    observerInstalled_ = false;
}

} // namespace auralis::ui
