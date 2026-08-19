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
    auralis::core::Logger::setObserver([this](QtMsgType type, const QString& category, const QString& message) {
        QMetaObject::invokeMethod(
            this,
            [this, type, category, message]() { appendFromLogger(type, category, message); },
            Qt::QueuedConnection);
    });
}

DiagnosticsLogModel::~DiagnosticsLogModel()
{
    auralis::core::Logger::setObserver({});
}

int DiagnosticsLogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return visibleIndices().size();
}

QVariant DiagnosticsLogModel::data(const QModelIndex& index, int role) const
{
    const QVector<int> visible = visibleIndices();
    if (!index.isValid() || index.row() < 0 || index.row() >= visible.size()) {
        return {};
    }
    const Entry& entry = entries_.at(visible.at(index.row()));
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
    endResetModel();
    emit filterChanged();
}

int DiagnosticsLogModel::visibleCount() const
{
    return visibleIndices().size();
}

QString DiagnosticsLogModel::visibleText() const
{
    QStringList lines;
    const QVector<int> visible = visibleIndices();
    for (int row : visible) {
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
    endResetModel();
    emit filterChanged();
}

void DiagnosticsLogModel::appendFromLogger(QtMsgType type, const QString& category, const QString& message)
{
    Entry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.severity = severityName(type);
    entry.category = category;
    entry.message = message;

    if (entries_.size() >= capacity_) {
        beginResetModel();
        entries_.removeFirst();
        entries_.push_back(entry);
        endResetModel();
        emit filterChanged();
        return;
    }

    if (matches(entry)) {
        const int visibleBefore = visibleIndices().size();
        beginInsertRows(QModelIndex(), visibleBefore, visibleBefore);
        entries_.push_back(entry);
        endInsertRows();
        emit filterChanged();
        return;
    }
    entries_.push_back(entry);
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

QVector<int> DiagnosticsLogModel::visibleIndices() const
{
    QVector<int> rows;
    rows.reserve(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        if (matches(entries_.at(i))) {
            rows.push_back(i);
        }
    }
    return rows;
}

void DiagnosticsLogModel::trimLocked()
{
    while (entries_.size() > capacity_) {
        entries_.removeFirst();
    }
}

} // namespace auralis::ui
