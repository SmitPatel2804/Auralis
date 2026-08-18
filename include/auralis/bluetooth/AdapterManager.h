#pragma once

#include <auralis/bluetooth/BlueZTypes.h>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace auralis::bluetooth {

class AdapterManager final : public QObject {
    Q_OBJECT

public:
    explicit AdapterManager(QObject* parent = nullptr);

    void upsertAdapter(AdapterData adapter);
    void applyPropertyChanges(
        const QString& objectPath,
        const QVariantMap& changed,
        const QStringList& invalidated);
    void removeAdapter(const QString& objectPath);
    void reconcile(const QSet<QString>& liveObjectPaths);
    void clear();

    bool hasAdapter() const noexcept;
    QString selectedObjectPath() const;
    AdapterData selected() const;
    AdapterData adapterAt(const QString& objectPath) const;

signals:
    void selectedAdapterChanged();
    void selectedAdapterUpdated();

private:
    void selectBestAdapter();
    bool isPreferred(const QString& objectPath) const;

    QHash<QString, AdapterData> adapters_;
    QString selectedPath_;
};

} // namespace auralis::bluetooth
