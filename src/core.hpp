#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

namespace mini {
std::wstring lower(std::wstring s);
std::wstring trim(std::wstring s);
std::wstring format_number(long double value, int precision = 10);
int fuzzy_score(const std::wstring& query, const std::wstring& candidate);
// Inputs are already lowercased; no per-candidate allocation or normalization.
int fuzzy_score_normalized(std::wstring_view query, std::wstring_view candidate);
bool contains_folded(std::wstring_view text, std::wstring_view normalizedQuery);
struct Calculation { std::wstring answer; bool error = false; };
std::optional<Calculation> calculate(const std::wstring& input);
struct Conversion { std::wstring answer; std::wstring detail; };
std::optional<Conversion> convert_units(const std::wstring& input);
struct CurrencyQuery { std::wstring amount, from, to; };
// A recognized currency without "to"/"in" uses defaultCurrency when provided.
// Explicit targets always win; incomplete explicit conversions never fall back.
std::optional<CurrencyQuery> parse_currency(const std::wstring& input, const std::wstring& defaultCurrency = L"");
std::optional<std::wstring> currency_result(const std::wstring& amount, const std::wstring& rate, int places);
std::optional<std::wstring> currency_cross_result(const std::wstring& amount,const std::wstring& fromUsdRate,const std::wstring& toUsdRate,int places);
enum class EmojiCategory : uint8_t { Smileys, People, Animals, Food, Travel, Activities, Objects, Symbols, Flags, Count };
inline constexpr int emoji_category_count = static_cast<int>(EmojiCategory::Count);
std::wstring_view emoji_category_name(EmojiCategory category);
struct Emoji {
    const wchar_t* glyph;
    const wchar_t* name;
    const wchar_t* keywords;
    EmojiCategory category = EmojiCategory::Smileys;
};
const std::vector<Emoji>& emojis();
// Bounded JSON reader used for the public currency API, never evaluated as code.
struct Json {
    enum Kind { Null, String, Number, Object, Array, Bool } kind = Null;
    std::string text;
    std::vector<std::pair<std::string, Json>> members;
    const Json* get(const std::string& key) const;
};
std::optional<Json> parse_json(const std::string& input);
}
