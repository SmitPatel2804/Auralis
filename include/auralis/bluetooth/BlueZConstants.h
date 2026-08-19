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
inline constexpr QStringView kAgentManagerInterface = u"org.bluez.AgentManager1";
inline constexpr QStringView kAgentInterface = u"org.bluez.Agent1";
inline constexpr QStringView kAgentManagerPath = u"/org/bluez";
inline constexpr QStringView kAuralisAgentPath = u"/auralis/agent";
inline constexpr QStringView kAgentCapabilityKeyboardDisplay = u"KeyboardDisplay";

inline constexpr QStringView kMethodGetManagedObjects = u"GetManagedObjects";
inline constexpr QStringView kMethodStartDiscovery = u"StartDiscovery";
inline constexpr QStringView kMethodStopDiscovery = u"StopDiscovery";
inline constexpr QStringView kMethodPair = u"Pair";
inline constexpr QStringView kMethodCancelPairing = u"CancelPairing";
inline constexpr QStringView kMethodConnect = u"Connect";
inline constexpr QStringView kMethodDisconnect = u"Disconnect";
inline constexpr QStringView kMethodRemoveDevice = u"RemoveDevice";
inline constexpr QStringView kMethodSet = u"Set";
inline constexpr QStringView kMethodRegisterAgent = u"RegisterAgent";
inline constexpr QStringView kMethodRequestDefaultAgent = u"RequestDefaultAgent";
inline constexpr QStringView kMethodUnregisterAgent = u"UnregisterAgent";
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
inline constexpr QStringView kErrorAlreadyConnected = u"org.bluez.Error.AlreadyConnected";
inline constexpr QStringView kErrorNotConnected = u"org.bluez.Error.NotConnected";
inline constexpr QStringView kErrorAuthenticationCanceled = u"org.bluez.Error.AuthenticationCanceled";
inline constexpr QStringView kErrorAuthenticationFailed = u"org.bluez.Error.AuthenticationFailed";
inline constexpr QStringView kErrorAuthenticationRejected = u"org.bluez.Error.AuthenticationRejected";
inline constexpr QStringView kErrorAuthenticationTimeout = u"org.bluez.Error.AuthenticationTimeout";
inline constexpr QStringView kErrorConnectionAttemptFailed = u"org.bluez.Error.ConnectionAttemptFailed";
inline constexpr QStringView kErrorNotSupported = u"org.bluez.Error.NotSupported";
inline constexpr QStringView kErrorInvalidArguments = u"org.bluez.Error.InvalidArguments";
inline constexpr QStringView kAgentErrorRejected = u"org.bluez.Error.Rejected";
inline constexpr QStringView kAgentErrorCanceled = u"org.bluez.Error.Canceled";

} // namespace auralis::bluetooth::bluez
