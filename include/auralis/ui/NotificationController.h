#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>

namespace auralis::ui {

class NotificationController final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY countChanged)
    Q_PROPERTY(QString latestErrorText READ latestErrorText NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        SeverityRole,
        TitleRole,
        MessageRole,
        StickyRole,
        TimestampRole
    };

    explicit NotificationController(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int warningCount() const;
    QString latestErrorText() const;

    Q_INVOKABLE void postInfo(const QString& title, const QString& message);
    Q_INVOKABLE void postWarning(const QString& title, const QString& message);
    Q_INVOKABLE void postError(const QString& title, const QString& message);
    Q_INVOKABLE void dismiss(const QString& id);
    Q_INVOKABLE void dismissLatest();
    Q_INVOKABLE void clearNonSticky();

signals:
    void countChanged();

private:
    struct Entry {
        QString id;
        QString severity;
        QString title;
        QString message;
        bool sticky = false;
        QDateTime timestamp;
    };

    void post(const QString& severity, const QString& title, const QString& message, bool sticky);

    QVector<Entry> entries_;
    int nextId_ = 1;
};

} // namespace auralis::ui
