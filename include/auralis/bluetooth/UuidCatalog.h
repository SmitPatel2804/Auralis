#pragma once

#include <QString>

namespace auralis::bluetooth {

class UuidCatalog {
public:
    static QString friendlyName(const QString& uuid);
};

} // namespace auralis::bluetooth
