#pragma once

// Copy overrides to ota_config.local.h. That file is intentionally ignored.
#if __has_include("ota_config.local.h")
#include "ota_config.local.h"
#endif

#ifndef WIO_OTA_ENABLED
#define WIO_OTA_ENABLED 0
#endif

#ifndef WIO_OTA_APN
#define WIO_OTA_APN "soracom.io"
#endif

#ifndef WIO_OTA_MANIFEST_HOST
#define WIO_OTA_MANIFEST_HOST "100.127.100.127"
#endif

#ifndef WIO_OTA_MANIFEST_PORT
#define WIO_OTA_MANIFEST_PORT 80
#endif

#ifndef WIO_OTA_MANIFEST_PATH
#define WIO_OTA_MANIFEST_PATH "/v1/userdata"
#endif

#ifndef WIO_OTA_FIRMWARE_HOST
#define WIO_OTA_FIRMWARE_HOST "100.127.111.48"
#endif

#ifndef WIO_OTA_FIRMWARE_PORT
#define WIO_OTA_FIRMWARE_PORT 80
#endif

#ifndef WIO_OTA_CURRENT_VERSION
#define WIO_OTA_CURRENT_VERSION 1
#endif

#ifndef WIO_OTA_AUTO_APPLY
#define WIO_OTA_AUTO_APPLY 0
#endif
