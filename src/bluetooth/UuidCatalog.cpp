#include <auralis/bluetooth/UuidCatalog.h>

#include <QHash>

namespace auralis::bluetooth {

QString UuidCatalog::friendlyName(const QString& uuid)
{
    static const QHash<QString, QString> known = {
        {QStringLiteral("0000110a-0000-1000-8000-00805f9b34fb"), QStringLiteral("A2DP Source")},
        {QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb"), QStringLiteral("A2DP Sink")},
        {QStringLiteral("0000110c-0000-1000-8000-00805f9b34fb"), QStringLiteral("AVRCP Target")},
        {QStringLiteral("0000110e-0000-1000-8000-00805f9b34fb"), QStringLiteral("AVRCP Controller")},
        {QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb"), QStringLiteral("Handsfree")},
        {QStringLiteral("00001108-0000-1000-8000-00805f9b34fb"), QStringLiteral("Headset")},
        {QStringLiteral("0000110d-0000-1000-8000-00805f9b34fb"), QStringLiteral("Headset AG")},
        {QStringLiteral("00001112-0000-1000-8000-00805f9b34fb"), QStringLiteral("HID Host")},
        {QStringLiteral("00001800-0000-1000-8000-00805f9b34fb"), QStringLiteral("Generic Access")},
        {QStringLiteral("00001801-0000-1000-8000-00805f9b34fb"), QStringLiteral("Generic Attribute")},
        {QStringLiteral("0000180f-0000-1000-8000-00805f9b34fb"), QStringLiteral("Battery Service")},
    };
    const QString normalized = uuid.toLower();
    return known.value(normalized);
}

} // namespace auralis::bluetooth
