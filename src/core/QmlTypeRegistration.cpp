#include <auralis/core/QmlTypeRegistration.h>

#include <auralis/audio/AudioRoute.h>
#include <auralis/bluetooth/PairingRequest.h>
#include <auralis/session/SessionTypes.h>

#include <QtQml>

namespace auralis::core {

void registerAuralisQmlTypes()
{
    qmlRegisterUncreatableMetaObject(
        auralis::session::staticMetaObject,
        "Auralis",
        1,
        0,
        "Session",
        QStringLiteral("Session enums only"));
    qmlRegisterUncreatableMetaObject(
        auralis::bluetooth::staticMetaObject,
        "Auralis",
        1,
        0,
        "Pairing",
        QStringLiteral("Pairing enums only"));
    qmlRegisterUncreatableMetaObject(
        auralis::audio::staticMetaObject,
        "Auralis",
        1,
        0,
        "Route",
        QStringLiteral("Route enums only"));
}

} // namespace auralis::core
