#pragma once

#include <QHash>
#include <QString>

#include <optional>

namespace auralis::audio {

class PipeWireProperties {
public:
    PipeWireProperties() = default;
    explicit PipeWireProperties(QHash<QString, QString> values);

    static PipeWireProperties fromHash(QHash<QString, QString> values);

    QString value(const QString& key, const QString& fallback = {}) const;
    std::optional<QString> optionalValue(const QString& key) const;
    std::optional<quint64> uintValue(const QString& key) const;
    std::optional<bool> boolValue(const QString& key) const;
    bool contains(const QString& key) const;
    bool isEmpty() const noexcept;
    QHash<QString, QString> toHash() const;

    void merge(const PipeWireProperties& other);

private:
    QHash<QString, QString> values_;
};

} // namespace auralis::audio
