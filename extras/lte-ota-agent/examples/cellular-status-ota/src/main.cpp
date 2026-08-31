// Share the implementation with the Arduino IDE sketch.
// PlatformIO supplies APP_VERSION / WIO_OTA_SECURE through build_flags.
// Keep dependency headers here: PlatformIO's LDF does not scan included .ino files.
#include <Adafruit_TinyUSB.h>
#include <WioCellular.h>
#include <WioOtaAgent.h>
#if defined(WIO_OTA_SECURE)
#include <WioOtaVersionStore.h>
#endif
#include "../CellularStatusOta/CellularStatusOta.ino"
