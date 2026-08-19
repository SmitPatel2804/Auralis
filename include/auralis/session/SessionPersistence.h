#pragma once

#include <auralis/session/AuralisSession.h>

#include <QString>
#include <QVector>

namespace auralis::session {

struct SessionPersistenceDocument {
    int schemaVersion = 1;
    QVector<AuralisSession> sessions;
};

class SessionPersistence {
public:
    explicit SessionPersistence(QString filePath);

    [[nodiscard]] QString filePath() const;
    [[nodiscard]] bool save(const SessionPersistenceDocument& document, QString* error = nullptr) const;
    [[nodiscard]] SessionPersistenceDocument load(QString* error = nullptr) const;

private:
    QString filePath_;
};

} // namespace auralis::session
