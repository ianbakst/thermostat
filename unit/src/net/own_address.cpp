#include "net/own_address.h"

#include <WiFi.h>

bool isOwnHost(const String& host, const char* name) {
  // Hostnames aren't case-sensitive: OfficeHVAC.local is officehvac.local.
  return host.equalsIgnoreCase(String(name) + ".local") || host == WiFi.localIP().toString();
}

bool isOwnOrigin(const String& origin, const char* name) {
  static const char* SCHEME = "http://";
  return origin.startsWith(SCHEME) && isOwnHost(origin.substring(strlen(SCHEME)), name);
}
