#pragma once

// OTA server AP naming, shared by the server, the OTA-mode receiver, and the
// normal-mode auto-update scan. Hardware-free so it's natively testable.

#include <stdlib.h>
#include <string.h>

namespace Scarfnet {

// A server running firmware vN advertises the SSID "scarfnet-ota-vN".
static const char* const kOtaSsidPrefix = "scarfnet-ota-v";

// Firmware version advertised by an OTA server SSID, or -1 if `ssid` isn't one.
inline int otaServerVersionFromSsid(const char* ssid)
{
    size_t n = strlen(kOtaSsidPrefix);
    if (strncmp(ssid, kOtaSsidPrefix, n) != 0) return -1;
    const char* digits = ssid + n;
    if (*digits == '\0') return -1;
    for (const char* p = digits; *p; ++p)
        if (*p < '0' || *p > '9') return -1;
    return atoi(digits);
}

} // namespace Scarfnet
