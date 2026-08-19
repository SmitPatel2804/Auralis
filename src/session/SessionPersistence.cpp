#include <auralis/session/SessionPersistence.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace auralis::session {
namespace {

constexpr int kSchemaVersion = 1;

double clampVolume(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

QJsonObject deviceToJson(const SessionDevice& device)
{
    QJsonObject object;
    object.insert(QStringLiteral("deviceId"), device.deviceId);
    object.insert(QStringLiteral("role"), toString(device.role));
    object.insert(QStringLiteral("enabled"), device.enabled);
    object.insert(QStringLiteral("volume"), device.volumeTrim);
    object.insert(QStringLiteral("muted"), device.muted);
    return object;
}

std::optional<SessionDevice> deviceFromJson(const QJsonObject& object)
{
    const QString deviceId = object.value(QStringLiteral("deviceId")).toString().trimmed().toUpper();
    if (deviceId.isEmpty()) {
        return std::nullopt;
    }
    SessionDevice device;
    device.deviceId = deviceId;
    bool ok = false;
    device.role = sessionDeviceRoleFromString(object.value(QStringLiteral("role")).toString(), &ok);
    if (!ok) {
        device.role = SessionDeviceRole::Unspecified;
    }
    device.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    device.volumeTrim = clampVolume(object.value(QStringLiteral("volume")).toDouble(1.0));
    device.muted = object.value(QStringLiteral("muted")).toBool(false);
    return device;
}

QJsonObject sessionToJson(const AuralisSession& session)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), session.id);
    object.insert(QStringLiteral("name"), session.name);
    object.insert(QStringLiteral("source"), session.sourceId);
    QJsonArray devices;
    for (const SessionDevice& device : session.devices) {
        devices.append(deviceToJson(device));
    }
    object.insert(QStringLiteral("devices"), devices);
    object.insert(QStringLiteral("groupVolume"), session.groupVolume);
    object.insert(QStringLiteral("muted"), session.muted);
    object.insert(QStringLiteral("autoReconnect"), session.autoReconnect);
    object.insert(QStringLiteral("recoveryPolicy"), toString(session.recoveryPolicy));
    object.insert(QStringLiteral("createdAt"), session.createdAt.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("updatedAt"), session.updatedAt.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("lastUsedAt"), session.lastUsedAt.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("restoreIntent"), session.restoreIntent);
    return object;
}

std::optional<AuralisSession> sessionFromJson(const QJsonObject& object)
{
    const QString id = object.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        return std::nullopt;
    }
    AuralisSession session;
    session.id = id;
    session.name = object.value(QStringLiteral("name")).toString().trimmed();
    if (session.name.isEmpty()) {
        session.name = QStringLiteral("Session");
    }
    session.sourceId = object.value(QStringLiteral("source")).toString().trimmed();
    const QJsonArray devices = object.value(QStringLiteral("devices")).toArray();
    QSet<QString> seenDevices;
    for (const QJsonValue& value : devices) {
        if (!value.isObject()) {
            continue;
        }
        const std::optional<SessionDevice> device = deviceFromJson(value.toObject());
        if (!device.has_value() || seenDevices.contains(device->deviceId)) {
            continue;
        }
        seenDevices.insert(device->deviceId);
        session.devices.push_back(*device);
    }
    session.groupVolume = clampVolume(object.value(QStringLiteral("groupVolume")).toDouble(1.0));
    session.muted = object.value(QStringLiteral("muted")).toBool(false);
    session.autoReconnect = object.value(QStringLiteral("autoReconnect")).toBool(true);
    bool ok = false;
    session.recoveryPolicy = recoveryPolicyFromString(object.value(QStringLiteral("recoveryPolicy")).toString(), &ok);
    if (!ok) {
        session.recoveryPolicy = RecoveryPolicy::ReconnectAndRestore;
    }
    session.createdAt = QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    session.updatedAt = QDateTime::fromString(object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
    session.lastUsedAt = QDateTime::fromString(object.value(QStringLiteral("lastUsedAt")).toString(), Qt::ISODateWithMs);
    if (!session.createdAt.isValid()) {
        session.createdAt = QDateTime::currentDateTimeUtc();
    }
    if (!session.updatedAt.isValid()) {
        session.updatedAt = session.createdAt;
    }
    session.restoreIntent = object.value(QStringLiteral("restoreIntent")).toBool(false);
    session.state = SessionState::Idle;
    return session;
}

} // namespace

SessionPersistence::SessionPersistence(QString filePath)
    : filePath_(std::move(filePath))
{
}

QString SessionPersistence::filePath() const
{
    return filePath_;
}

bool SessionPersistence::save(const SessionPersistenceDocument& document, QString* error) const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), document.schemaVersion);
    QJsonArray sessions;
    for (const AuralisSession& session : document.sessions) {
        sessions.append(sessionToJson(session));
    }
    root.insert(QStringLiteral("sessions"), sessions);

    const QFileInfo info(filePath_);
    QDir parentDir(info.absolutePath());
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create persistence directory: %1").arg(info.absolutePath());
        }
        return false;
    }

    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to open persistence file for writing: %1").arg(filePath_);
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to commit persistence file: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

SessionPersistenceDocument SessionPersistence::load(QString* error) const
{
    SessionPersistenceDocument document;
    document.schemaVersion = kSchemaVersion;
    if (!QFile::exists(filePath_)) {
        return document;
    }
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to open persistence file for reading: %1").arg(filePath_);
        }
        return document;
    }
    const QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), const_cast<QJsonParseError*>(&parseError));
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("Invalid session persistence JSON: %1").arg(parseError.errorString());
        }
        return document;
    }
    const QJsonObject root = json.object();
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(kSchemaVersion);
    if (schemaVersion > kSchemaVersion) {
        if (error != nullptr) {
            *error = QStringLiteral("Unsupported session schema version: %1").arg(schemaVersion);
        }
        return document;
    }
    document.schemaVersion = schemaVersion;
    QSet<QString> seenIds;
    for (const QJsonValue& value : root.value(QStringLiteral("sessions")).toArray()) {
        if (!value.isObject()) {
            continue;
        }
        const std::optional<AuralisSession> session = sessionFromJson(value.toObject());
        if (!session.has_value() || seenIds.contains(session->id)) {
            continue;
        }
        seenIds.insert(session->id);
        document.sessions.push_back(*session);
    }
    return document;
}

} // namespace auralis::session
