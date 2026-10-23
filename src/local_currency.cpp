#include "local_currency.hpp"
#include <windows.h>

namespace mini {
namespace {
std::wstring iso_currency(const wchar_t* value) {
    std::wstring code(value);
    if (code.size() != 3) return {};
    for (auto& ch : code) {
        if (ch >= L'a' && ch <= L'z') ch -= L'a' - L'A';
        if (ch < L'A' || ch > L'Z') return {};
    }
    return code;
}
}

std::wstring local_currency_code() {
    // Windows 10 1709+: prefer the configured country to the formatting locale.
    // For example, English (United States) formats plus Indonesia home location
    // must yield IDR. GEO_CURRENCYCODE maps the country using Windows' own data.
    // https://learn.microsoft.com/windows/win32/api/winnls/nf-winnls-getuserdefaultgeoname
    // https://learn.microsoft.com/windows/win32/api/winnls/ne-winnls-sysgeotype
    wchar_t country[16]{};
    wchar_t currency[16]{};
    if (GetUserDefaultGeoName(country, 16) > 1) {
        if (GetGeoInfoEx(country, GEO_CURRENCYCODE, currency, 16) > 1)
            return iso_currency(currency);
        // A known country with no currency must not silently become USD merely
        // because the user chose US English formatting.
        return {};
    }

    // The locale is a fallback only when Windows could not provide a home
    // location. LOCALE_NOUSEROVERRIDE asks for its standard ISO currency code.
    if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT,
                        LOCALE_SINTLSYMBOL | LOCALE_NOUSEROVERRIDE,
                        currency, 16) > 1)
        return iso_currency(currency);
    return {};
}
}
