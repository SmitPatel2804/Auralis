#pragma once

#include <QStringView>

namespace auralis::bluetooth::bluez {

inline constexpr QStringView kService = u"org.bluez";
inline constexpr QStringView kObjectManagerInterface = u"org.freedesktop.DBus.ObjectManager";
inline constexpr QStringView kPropertiesInterface = u"org.freedesktop.DBus.Properties";
inline constexpr QStringView kAdapterInterface = u"org.bluez.Adapter1";
inline constexpr QStringView kDeviceInterface = u"org.bluez.Device1";
inline constexpr QStringView kRootPath = u"/";
inline constexpr QStringView kPreferredAdapterPath = u"/org/bluez/hci0";

inline constexpr QStringView kMethodGetManagedObjects = u"GetManagedObjects";
inline constexpr QStringView kMethodStartDiscovery = u"StartDiscovery";
inline constexpr QStringView kMethodStopDiscovery = u"StopDiscovery";
inline constexpr QStringView kSignalInterfacesAdded = u"InterfacesAdded";
inline constexpr QStringView kSignalInterfacesRemoved = u"InterfacesRemoved";
inline constexpr QStringView kSignalPropertiesChanged = u"PropertiesChanged";

inline constexpr QStringView kPropAddress = u"Address";
inline constexpr QStringView kPropAddressType = u"AddressType";
inline constexpr QStringView kPropName = u"Name";
inline constexpr QStringView kPropAlias = u"Alias";
inline constexpr QStringView kPropIcon = u"Icon";
inline constexpr QStringView kPropClass = u"Class";
inline constexpr QStringView kPropAppearance = u"Appearance";
inline constexpr QStringView kPropUuids = u"UUIDs";
inline constexpr QStringView kPropPaired = u"Paired";
inline constexpr QStringView kPropBonded = u"Bonded";
inline constexpr QStringView kPropConnected = u"Connected";
inline constexpr QStringView kPropTrusted = u"Trusted";
inline constexpr QStringView kPropBlocked = u"Blocked";
inline constexpr QStringView kPropAdapter = u"Adapter";
inline constexpr QStringView kPropLegacyPairing = u"LegacyPairing";
inline constexpr QStringView kPropModalias = u"Modalias";
inline constexpr QStringView kPropRssi = u"RSSI";
inline constexpr QStringView kPropTxPower = u"TxPower";
inline constexpr QStringView kPropManufacturerData = u"ManufacturerData";
inline constexpr QStringView kPropServiceData = u"ServiceData";
inline constexpr QStringView kPropServicesResolved = u"ServicesResolved";
inline constexpr QStringView kPropPowered = u"Powered";
inline constexpr QStringView kPropDiscoverable = u"Discoverable";
inline constexpr QStringView kPropPairable = u"Pairable";
inline constexpr QStringView kPropDiscovering = u"Discovering";

inline constexpr QStringView kErrorNotReady = u"org.bluez.Error.NotReady";
inline constexpr QStringView kErrorFailed = u"org.bluez.Error.Failed";
inline constexpr QStringView kErrorInProgress = u"org.bluez.Error.InProgress";
inline constexpr QStringView kErrorNotAuthorized = u"org.bluez.Error.NotAuthorized";

} // namespace auralis::bluetooth::bluez
