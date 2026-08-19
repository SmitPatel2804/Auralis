#include <auralis/ui/NotificationController.h>

#include <auralis/core/LoggingCategories.h>

namespace auralis::ui {

NotificationController::NotificationController(QObject* parent)
    : QAbstractListModel(parent)
{
}

int NotificationController::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return entries_.size();
}

QVariant NotificationController::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return {};
    }
    const Entry& entry = entries_.at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case SeverityRole:
        return entry.severity;
    case TitleRole:
        return entry.title;
    case MessageRole:
        return entry.message;
    case StickyRole:
        return entry.sticky;
    case TimestampRole:
        return entry.timestamp;
    case Qt::DisplayRole:
        return entry.title;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationController::roleNames() const
{
    return {
        {IdRole, "notificationId"},
        {SeverityRole, "severity"},
        {TitleRole, "title"},
        {MessageRole, "message"},
        {StickyRole, "sticky"},
        {TimestampRole, "timestamp"},
    };
}

int NotificationController::count() const
{
    return entries_.size();
}

int NotificationController::warningCount() const
{
    int n = 0;
    for (const Entry& entry : entries_) {
        if (entry.severity == QLatin1String("warning") || entry.severity == QLatin1String("error")) {
            ++n;
        }
    }
    return n;
}

QString NotificationController::latestErrorText() const
{
    for (int i = entries_.size() - 1; i >= 0; --i) {
        if (entries_.at(i).severity == QLatin1String("error")) {
            const Entry& entry = entries_.at(i);
            if (entry.title.isEmpty()) {
                return entry.message;
            }
            if (entry.message.isEmpty()) {
                return entry.title;
            }
            return entry.title + QStringLiteral(": ") + entry.message;
        }
    }
    return {};
}

void NotificationController::postInfo(const QString& title, const QString& message)
{
    post(QStringLiteral("info"), title, message, false);
}

void NotificationController::postWarning(const QString& title, const QString& message)
{
    post(QStringLiteral("warning"), title, message, true);
}

void NotificationController::postError(const QString& title, const QString& message)
{
    qCWarning(auralisUi) << "Gui.Notification" << title << message;
    post(QStringLiteral("error"), title, message, true);
}

void NotificationController::dismiss(const QString& id)
{
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_.at(i).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            entries_.removeAt(i);
            endRemoveRows();
            emit countChanged();
            return;
        }
    }
}

void NotificationController::dismissLatest()
{
    if (entries_.isEmpty()) {
        return;
    }
    beginRemoveRows(QModelIndex(), entries_.size() - 1, entries_.size() - 1);
    entries_.removeLast();
    endRemoveRows();
    emit countChanged();
}

void NotificationController::clearNonSticky()
{
    for (int i = entries_.size() - 1; i >= 0; --i) {
        if (!entries_.at(i).sticky) {
            beginRemoveRows(QModelIndex(), i, i);
            entries_.removeAt(i);
            endRemoveRows();
        }
    }
    emit countChanged();
}

void NotificationController::post(
    const QString& severity,
    const QString& title,
    const QString& message,
    bool sticky)
{
    Entry entry;
    entry.id = QString::number(nextId_++);
    entry.severity = severity;
    entry.title = title;
    entry.message = message;
    entry.sticky = sticky;
    entry.timestamp = QDateTime::currentDateTime();
    const int row = entries_.size();
    beginInsertRows(QModelIndex(), row, row);
    entries_.push_back(entry);
    endInsertRows();
    emit countChanged();
}

} // namespace auralis::ui
