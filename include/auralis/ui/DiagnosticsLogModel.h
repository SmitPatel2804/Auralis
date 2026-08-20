#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>

#include <QtGlobal>

namespace auralis::ui {

class DiagnosticsLogModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int capacity READ capacity WRITE setCapacity NOTIFY capacityChanged)
    Q_PROPERTY(QString severityFilter READ severityFilter WRITE setSeverityFilter NOTIFY filterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filterChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY filterChanged)
    Q_PROPERTY(bool captureEnabled READ captureEnabled WRITE setCaptureEnabled NOTIFY captureEnabledChanged)

public:
    enum Role {
        TimestampRole = Qt::UserRole + 1,
        SeverityRole,
        CategoryRole,
        MessageRole
    };

    explicit DiagnosticsLogModel(QObject* parent = nullptr);
    ~DiagnosticsLogModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int capacity() const noexcept;
    void setCapacity(int value);
    QString severityFilter() const;
    void setSeverityFilter(const QString& value);
    QString categoryFilter() const;
    void setCategoryFilter(const QString& value);
    int visibleCount() const;
    bool captureEnabled() const noexcept;

    Q_INVOKABLE QString visibleText() const;
    Q_INVOKABLE bool copyVisibleToClipboard() const;
    Q_INVOKABLE void clear();

    void appendFromLogger(QtMsgType type, const QString& category, const QString& message);

public slots:
    void setCaptureEnabled(bool enabled);

signals:
    void capacityChanged();
    void filterChanged();
    void captureEnabledChanged();

private:
    struct Entry {
        QDateTime timestamp;
        QString severity;
        QString category;
        QString message;
    };

    bool matches(const Entry& entry) const;
    void rebuildVisibleRows();
    void trimLocked();
    void installObserver();
    void removeObserver();

    QVector<Entry> entries_;
    QVector<int> visibleRows_;
    int capacity_ = 500;
    QString severityFilter_;
    QString categoryFilter_;
    bool captureEnabled_ = false;
    bool observerInstalled_ = false;
};

} // namespace auralis::ui
