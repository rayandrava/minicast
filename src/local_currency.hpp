#pragma once
#include <string>

namespace mini {
// Reads Windows' configured home country, independently of display language and
// number/date formatting. Uses the format locale only when home geography is
// unavailable. Returns an empty string when no currency can be determined.
// This is a local settings lookup: no IP geolocation, network, or permission UI.
std::wstring local_currency_code();
}
