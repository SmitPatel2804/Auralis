#include <auralis/audio/PipeWireProperties.h>

namespace auralis::audio {

PipeWireProperties::PipeWireProperties(QHash<QString, QString> values)
    : values_(std::move(values))
{
}

PipeWireProperties PipeWireProperties::fromHash(QHash<QString, QString> values)
{
    return PipeWireProperties(std::move(values));
}

QString PipeWireProperties::value(const QString& key, const QString& fallback) const
{
    const auto it = values_.constFind(key);
    if (it == values_.cend() || it->isEmpty()) {
        return fallback;
    }
    return *it;
}

std::optional<QString> PipeWireProperties::optionalValue(const QString& key) const
{
    const auto it = values_.constFind(key);
    if (it == values_.cend() || it->isEmpty()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<quint64> PipeWireProperties::uintValue(const QString& key) const
{
    const std::optional<QString> raw = optionalValue(key);
    if (!raw.has_value()) {
        return std::nullopt;
    }
    bool ok = false;
    const quint64 parsed = raw->toULongLong(&ok, 10);
    if (!ok) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<bool> PipeWireProperties::boolValue(const QString& key) const
{
    const std::optional<QString> raw = optionalValue(key);
    if (!raw.has_value()) {
        return std::nullopt;
    }
    const QString lowered = raw->trimmed().toLower();
    if (lowered == QLatin1String("true") || lowered == QLatin1String("yes") || lowered == QLatin1String("1")) {
        return true;
    }
    if (lowered == QLatin1String("false") || lowered == QLatin1String("no") || lowered == QLatin1String("0")) {
        return false;
    }
    return std::nullopt;
}

bool PipeWireProperties::contains(const QString& key) const
{
    return values_.contains(key);
}

bool PipeWireProperties::isEmpty() const noexcept
{
    return values_.isEmpty();
}

QHash<QString, QString> PipeWireProperties::toHash() const
{
    return values_;
}

void PipeWireProperties::merge(const PipeWireProperties& other)
{
    for (auto it = other.values_.constBegin(); it != other.values_.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            values_.insert(it.key(), it.value());
        }
    }
}

} // namespace auralis::audio
