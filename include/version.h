#pragma once

#define FIRMWARE_VERSION 6

// HTTP Basic Auth credentials for OTA endpoints.
// Password reuses the mesh password (kMeshPassword from login.h) so there is
// one shared secret across the whole fleet. Both must match on server and receiver.
#define OTA_HTTP_USER "scarfnet"
