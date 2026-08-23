#pragma once

#include <auralis/audio/AudioSource.h>

#include <QAbstractListModel>
#include <QVector>

namespace auralis::audio {

class AudioSourceListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SourceIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        MediaClassRole,
        SourceTypeRole,
        ApplicationNameRole,
        AvailableRole,
        MonitorRole,
        PipeWireNodeIdRole
    };

    explicit AudioSourceListModel(QObject* parent = nullptr);

    void setSources(QVector<AudioSource> sources);
    QVector<AudioSource> sources() const;
    Q_INVOKABLE QString sourceIdAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<AudioSource> sources_;
};

} // namespace auralis::audio
