#include "core.hpp"
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace mini {
std::wstring lower(std::wstring s) { for (auto& c : s) c = towlower(c); return s; }
std::wstring trim(std::wstring s) {
    auto a = s.find_first_not_of(L" \t\r\n");
    if (a == s.npos) return {};
    return s.substr(a, s.find_last_not_of(L" \t\r\n") - a + 1);
}
std::wstring format_number(long double n, int precision) {
    if (!std::isfinite(n)) return L"Out of range";
    if (n == 0) n = 0;
    std::wostringstream out;
    out.imbue(std::locale::classic());
    // Windows CRT long-double formatting differs from Clang's 80-bit ABI.
    // Keep extended precision during calculation and format a portable double.
    out << std::setprecision(precision) << static_cast<double>(n);
    return out.str();
}
int fuzzy_score(const std::wstring& q0, const std::wstring& c0) {
    return fuzzy_score_normalized(lower(trim(q0)), lower(c0));
}
int fuzzy_score_normalized(std::wstring_view q, std::wstring_view c) {
    if (q.empty()) return 1;
    if (q == c) return 10000;
    if (c.starts_with(q)) return 8000 - std::min<int>(c.size(), 500);
    auto at = c.find(q);
    if (at != c.npos) return 6000 - static_cast<int>(std::min<size_t>(at, 4000));
    size_t pos = 0; int score = 1000, previous = -2;
    for (wchar_t ch : q) {
        if (iswspace(ch)) continue;
        auto found = c.find(ch, pos);
        if (found == c.npos) return 0;
        if (found == 0 || !iswalnum(c[found - 1])) score += 60;
        if (static_cast<int>(found) == previous + 1) score += 30;
        score -= static_cast<int>(found - pos);
        previous = static_cast<int>(found); pos = found + 1;
    }
    return std::max(1, score);
}
bool contains_folded(std::wstring_view text, std::wstring_view query) {
    if (query.empty()) return true;
    if (query.size() > text.size()) return false;
    for (size_t i = 0; i <= text.size() - query.size(); ++i) {
        size_t n = 0;
        while (n < query.size() && towlower(text[i+n]) == query[n]) ++n;
        if (n == query.size()) return true;
    }
    return false;
}
struct Unit { const wchar_t* names; const wchar_t* label; int dimension; long double scale, offset; };
static const Unit units[] = {
    {L"m|meter|meters|metre|metres", L"m",0,1,0}, {L"km|kilometer|kilometers",L"km",0,1000,0},
    {L"cm|centimeter|centimeters",L"cm",0,.01L,0}, {L"mm|millimeter|millimeters",L"mm",0,.001L,0},
    {L"mi|mile|miles",L"mi",0,1609.344L,0}, {L"ft|foot|feet",L"ft",0,.3048L,0},
    {L"in|inch|inches",L"in",0,.0254L,0}, {L"yd|yard|yards",L"yd",0,.9144L,0},
    {L"kg|kilogram|kilograms",L"kg",1,1,0}, {L"g|gram|grams",L"g",1,.001L,0},
    {L"mg|milligram|milligrams",L"mg",1,.000001L,0}, {L"lb|lbs|pound|pounds",L"lb",1,.45359237L,0},
    {L"oz|ounce|ounces",L"oz (mass)",1,.028349523125L,0},
    {L"c|°c|celsius",L"°C",2,1,273.15L}, {L"f|°f|fahrenheit",L"°F",2,5.L/9,255.37222222222222222L},
    {L"k|kelvin",L"K",2,1,0},
    {L"l|liter|liters|litre|litres",L"L",3,1,0}, {L"ml|milliliter|milliliters",L"mL",3,.001L,0},
    {L"usgal|us gallon|us gallons",L"US gal",3,3.785411784L,0},
    {L"impgal|imperial gallon",L"imperial gal",3,4.54609L,0},
    {L"s|sec|second|seconds",L"s",4,1,0}, {L"min|minute|minutes",L"min",4,60,0},
    {L"h|hr|hour|hours",L"h",4,3600,0}, {L"d|day|days",L"days",4,86400,0},
    {L"b|byte|bytes",L"B",5,1,0}, {L"kb",L"kB",5,1000,0}, {L"mb",L"MB",5,1000000,0},
    {L"gb",L"GB",5,1000000000,0}, {L"tb",L"TB",5,1000000000000.L,0},
    {L"kib",L"KiB",5,1024,0}, {L"mib",L"MiB",5,1048576,0}, {L"gib",L"GiB",5,1073741824,0},
    {L"bit|bits",L"bits",5,.125L,0},
    {L"m/s|mps",L"m/s",6,1,0}, {L"km/h|kph|kmh",L"km/h",6,1.L/3.6L,0},
    {L"mph",L"mph",6,.44704L,0}, {L"kn|knot|knots",L"kn",6,1852.L/3600,0},
    {L"m2|m²|sqm",L"m²",7,1,0}, {L"ft2|ft²|sqft",L"ft²",7,.09290304L,0},
    {L"ha|hectare|hectares",L"ha",7,10000,0}, {L"acre|acres",L"acres",7,4046.8564224L,0},
    {L"rad|radian|radians",L"rad",8,1,0}, {L"deg|degree|degrees",L"°",8,.01745329251994329577L,0}
};
static const Unit* unit(const std::wstring& key) {
    for (const auto& u : units) {
        std::wstring_view aliases(u.names);
        while (!aliases.empty()) {
            auto end = aliases.find(L'|');
            if (aliases.substr(0, end) == key) return &u;
            if (end == aliases.npos) break;
            aliases.remove_prefix(end + 1);
        }
    }
    return nullptr;
}
std::optional<Conversion> convert_units(const std::wstring& input) {
    auto s = lower(trim(input)); if (s.size() > 256) return {};
    auto sep = s.find(L" to "); size_t nsep = 4;
    if (sep == s.npos) { sep = s.find(L" in "); nsep = 4; }
    if (sep == s.npos) return {};
    auto left = trim(s.substr(0,sep)), right = trim(s.substr(sep+nsep));
    wchar_t* end = nullptr; auto value = wcstold(left.c_str(), &end);
    if (end == left.c_str() || !std::isfinite(value) || std::abs(value) > 1e24L) return {};
    auto fromName = trim(end); auto from = unit(fromName), to = unit(right);
    // Mixed feet and inches, for example 5 ft 10 in to cm.
    if (fromName.starts_with(L"ft ")) {
        auto rest = trim(fromName.substr(3)); wchar_t* tail;
        long double inches = wcstold(rest.c_str(), &tail);
        if (tail != rest.c_str() && trim(tail) == L"in" && inches >= 0 && inches < 12) {
            value += (value < 0 ? -inches : inches)/12; from = unit(L"ft");
        }
    }
    if (!from || !to) return {};
    if (from->dimension != to->dimension) return Conversion{L"Incompatible units",L"Choose units that measure the same dimension"};
    auto base = value * from->scale + from->offset;
    if (from->dimension == 2 && base < -1e-10L) return Conversion{L"Below absolute zero",L"Temperature must be at least 0 K"};
    auto result = (base - to->offset)/to->scale;
    if (std::abs(result) < 1e-12L) result = 0;
    return Conversion{format_number(result) + L" " + to->label, format_number(value) + L" " + from->label + L" → " + to->label};
}
std::optional<CurrencyQuery> parse_currency(const std::wstring& input, const std::wstring& defaultCurrency) {
    auto s = lower(trim(input)); if (s.size() > 128) return {};
    if(s.starts_with(L"convert "))s=trim(s.substr(8));
    auto sep=s.find(L" to ");if(sep==s.npos)sep=s.find(L" in ");
    const bool useDefault = sep == s.npos;
    // trim() removes a trailing separator's last space. Do not turn a partially
    // typed explicit conversion back into an implicit one.
    if (useDefault && (defaultCurrency.empty() || s.ends_with(L" to") || s.ends_with(L" in"))) return {};
    auto left=trim(s.substr(0,sep));
    auto to=useDefault ? lower(trim(defaultCurrency)) : trim(s.substr(sep+4));
    std::wstring implicit;
    if(left.starts_with(L"$")){implicit=L"usd";left=trim(left.substr(1));}
    else if(left.starts_with(L"€")){implicit=L"eur";left=trim(left.substr(1));}
    else if(left.starts_with(L"£")){implicit=L"gbp";left=trim(left.substr(1));}
    size_t end=0;while(end<left.size()&&(iswdigit(left[end])||left[end]==L'.'||(end==0&&(left[end]==L'-'||left[end]==L'+'))))end++;
    auto amount=left.substr(0,end),from=trim(left.substr(end));if(from.empty())from=implicit;
    auto code=[](std::wstring name){const std::pair<const wchar_t*,const wchar_t*> aliases[]={
        {L"dollar",L"usd"},{L"dollars",L"usd"},{L"us dollar",L"usd"},{L"us dollars",L"usd"},
        {L"rupiah",L"idr"},{L"indonesian rupiah",L"idr"},{L"rp",L"idr"},
        {L"euro",L"eur"},{L"euros",L"eur"},{L"pound",L"gbp"},{L"pounds",L"gbp"},{L"yen",L"jpy"},
        {L"$",L"usd"},{L"€",L"eur"},{L"£",L"gbp"},{L"japanese yen",L"jpy"},
        {L"british pound",L"gbp"},{L"british pounds",L"gbp"},
        {L"yuan",L"cny"},{L"renminbi",L"cny"},{L"rmb",L"cny"},{L"won",L"krw"},
        {L"ringgit",L"myr"},{L"baht",L"thb"},{L"rupees",L"inr"},{L"rupee",L"inr"},
        {L"singapore dollar",L"sgd"},{L"singapore dollars",L"sgd"},
        {L"australian dollar",L"aud"},{L"australian dollars",L"aud"},
        {L"canadian dollar",L"cad"},{L"canadian dollars",L"cad"}};
        for(auto [alias,value]:aliases)if(name==alias)return std::wstring(value);return name;};
    from=code(from);to=code(to);
    if (from.size() != 3 || to.size() != 3) return {};
    for (auto ch : from+to) if (ch < L'a' || ch > L'z') return {};
    if (useDefault) {
        // This offline allowlist prevents ordinary search queries and units from
        // becoming currency cards. Codes are the public provider's currencies:
        // https://api.frankfurter.dev/v2/currencies (2026-10-23).
        // Explicit conversions remain open to new currencies added by the feed.
        constexpr std::wstring_view codes =
            L"aed afn all amd ang aoa ars aud awg azn bam bbd bdt bhd bif bmd bnd bob brl bsd btn bwp byn bzd "
            L"cad cdf chf clp cmd cnh cny cop crc cup cve czk djf dkk dop dzd egp ern etb eur fjd fkp gbp gel "
            L"ggp ghs gip gmd gnf gtq gyd hkd hnl htg huf idr ils imp inr iqd irr isk jep jmd jod jpy kes kgs "
            L"khr kmf kpw krw kwd kyd kzt lak lbp lkr lrd lsl lyd mad mdl mga mkd mmk mnt mop mru mur mvr mwk "
            L"mxn myr mzn nad ngn nio nok npr nzd omr pab pen pgk php pkr pln pyg qar ron rsd rub rwf sar sbd "
            L"scr sdg sek sgd shp sle sos srd ssp stn svc syp szl thb tjs tmt tnd top try ttd twd tzs uah ugx "
            L"usd uyu uzs ves vnd vuv wst xaf xag xau xcd xcg xdr xof xpd xpf xpt yer zar zmw zwg";
        const auto match = codes.find(from);
        if (match == codes.npos || match % 4 != 0) return {};
    }
    if (unit(from) && unit(to)) return {};
    int digits = 0, dots = 0;
    for (size_t i=0; i<amount.size(); ++i) {
        auto c = amount[i]; if (i == 0 && (c == L'-' || c == L'+')) continue;
        if (c == L'.') { if (++dots > 1) return {}; } else if (c >= L'0' && c <= L'9') ++digits; else return {};
    }
    if (!digits || digits > 18) return {};
    for (auto& c : from) c=towupper(c); for (auto& c : to) c=towupper(c);
    return CurrencyQuery{amount,from,to};
}
static bool decimal(const std::wstring& s, __int128& n, __int128& scale) {
    n=0; scale=1; bool frac=false, neg=false; int digits=0;
    for (size_t i=0;i<s.size();++i) {
        wchar_t c=s[i]; if (i==0 && (c==L'-'||c==L'+')) { neg=c==L'-'; continue; }
        if (c==L'.' && !frac) {frac=true; continue;}
        if (c<L'0'||c>L'9'||++digits>18) return false;
        n=n*10+c-L'0'; if(frac) scale*=10;
    }
    if(neg) n=-n; return digits>0;
}
std::optional<std::wstring> currency_result(const std::wstring& amount,const std::wstring& rate,int places) {
    __int128 a,as,r,rs; if(places<0||places>6||!decimal(amount,a,as)||!decimal(rate,r,rs)||r<=0) return {};
    // Restrict the amount to 12 significant digits so product * 10^6 fits signed 128-bit.
    if (a > 999999999999LL || a < -999999999999LL) return {};
    __int128 factor=1; for(int i=0;i<places;i++) factor*=10;
    bool neg=a<0; if(neg) a=-a;
    __int128 denom=as*rs, result=(a*r*factor + denom/2)/denom;
    std::wstring out; do {out.insert(out.begin(),L'0'+static_cast<int>(result%10)); result/=10;}while(result);
    if(places){ while(out.size()<=static_cast<size_t>(places))out.insert(out.begin(),L'0'); out.insert(out.size()-places,1,L'.'); }
    if(neg && out.find_first_not_of(L"0.")!=out.npos)out.insert(out.begin(),L'-');
    return out;
}
std::optional<std::wstring> currency_cross_result(const std::wstring& amount,const std::wstring& fromRate,const std::wstring& toRate,int places){
    __int128 a,as,f,fs,t,ts;if(places<0||places>6||!decimal(amount,a,as)||!decimal(fromRate,f,fs)||!decimal(toRate,t,ts)||f<=0||t<=0)return {};
    if(a>999999999999LL||a< -999999999999LL)return {};bool neg=a<0;if(neg)a=-a;
    __int128 factor=1;for(int i=0;i<places;i++)factor*=10;
    __int128 numerator[]={a,t,fs,factor},denominator[]={as,ts,f};
    auto gcd=[](__int128 x,__int128 y){while(y){auto r=x%y;x=y;y=r;}return x;};
    for(auto& n:numerator)for(auto& d:denominator){auto divisor=gcd(n,d);if(divisor){n/=divisor;d/=divisor;}}
    const unsigned __int128 maximum=(static_cast<unsigned __int128>(1)<<127)-1;__int128 n=1,d=1;
    for(auto v:numerator){if(v&&static_cast<unsigned __int128>(n)>maximum/static_cast<unsigned __int128>(v))return {};n*=v;}
    for(auto v:denominator){if(!v||static_cast<unsigned __int128>(d)>maximum/static_cast<unsigned __int128>(v))return {};d*=v;}
    __int128 result=n/d,remainder=n%d;if(remainder>=d/2+d%2)result++;
    std::wstring out;do{out.insert(out.begin(),L'0'+static_cast<int>(result%10));result/=10;}while(result);
    if(places){while(out.size()<=static_cast<size_t>(places))out.insert(out.begin(),L'0');out.insert(out.size()-places,1,L'.');}
    if(neg&&out.find_first_not_of(L"0.")!=out.npos)out.insert(out.begin(),L'-');return out;
}
std::wstring_view emoji_category_name(EmojiCategory category) {
    static constexpr std::wstring_view names[] = {
        L"Smileys & Emotion", L"People & Gestures", L"Animals & Nature", L"Food & Drink",
        L"Travel & Places", L"Activities", L"Objects", L"Symbols", L"Flags"
    };
    static_assert(sizeof(names) / sizeof(names[0]) == emoji_category_count);
    const auto index = static_cast<unsigned>(category);
    return index < emoji_category_count ? names[index] : std::wstring_view{};
}

const std::vector<Emoji>& emojis() {
    static const std::vector<Emoji> items = {
        {L"😀",L"Grinning face",L"happy smile joy", EmojiCategory::Smileys},{L"😃",L"Big smile",L"happy joy", EmojiCategory::Smileys},{L"😄",L"Smiling eyes",L"happy laugh", EmojiCategory::Smileys},
        {L"😁",L"Beaming face",L"grin smile", EmojiCategory::Smileys},{L"😆",L"Laughing",L"happy lol", EmojiCategory::Smileys},{L"😅",L"Sweat smile",L"nervous relief", EmojiCategory::Smileys},
        {L"😂",L"Tears of joy",L"laugh lol funny", EmojiCategory::Smileys},{L"🤣",L"Rolling laughing",L"rofl funny", EmojiCategory::Smileys},{L"🙂",L"Slight smile",L"happy", EmojiCategory::Smileys},
        {L"🙃",L"Upside down",L"sarcasm", EmojiCategory::Smileys},{L"😉",L"Wink",L"winking", EmojiCategory::Smileys},{L"😊",L"Blush",L"smile happy", EmojiCategory::Smileys},
        {L"😍",L"Heart eyes",L"love", EmojiCategory::Smileys},{L"🥰",L"Smiling hearts",L"love affection", EmojiCategory::Smileys},{L"😘",L"Kiss",L"love", EmojiCategory::Smileys},
        {L"😎",L"Sunglasses",L"cool", EmojiCategory::Smileys},{L"🤓",L"Nerd",L"glasses", EmojiCategory::Smileys},{L"🧐",L"Monocle",L"inspect", EmojiCategory::Smileys},
        {L"🤔",L"Thinking",L"hmm question", EmojiCategory::Smileys},{L"🤨",L"Raised eyebrow",L"skeptical", EmojiCategory::Smileys},{L"😐",L"Neutral",L"meh", EmojiCategory::Smileys},
        {L"😑",L"Expressionless",L"meh", EmojiCategory::Smileys},{L"🙄",L"Eye roll",L"annoyed", EmojiCategory::Smileys},{L"😏",L"Smirk",L"smug", EmojiCategory::Smileys},
        {L"😴",L"Sleeping",L"sleep tired", EmojiCategory::Smileys},{L"🥱",L"Yawning",L"tired bored", EmojiCategory::Smileys},{L"😮",L"Surprised",L"wow", EmojiCategory::Smileys},
        {L"😱",L"Screaming",L"fear shock", EmojiCategory::Smileys},{L"😢",L"Crying",L"sad tear", EmojiCategory::Smileys},{L"😭",L"Loudly crying",L"sad sob", EmojiCategory::Smileys},
        {L"🥹",L"Holding back tears",L"emotional grateful", EmojiCategory::Smileys},{L"🥺",L"Pleading",L"please puppy", EmojiCategory::Smileys},
        {L"😤",L"Huffing",L"angry frustrated", EmojiCategory::Smileys},{L"😡",L"Angry",L"mad rage", EmojiCategory::Smileys},{L"🤯",L"Mind blown",L"wow explosion", EmojiCategory::Smileys},
        {L"🤩",L"Star struck",L"excited", EmojiCategory::Smileys},{L"🥳",L"Party face",L"celebrate birthday", EmojiCategory::Smileys},{L"🤗",L"Hug",L"hugging", EmojiCategory::Smileys},
        {L"🤫",L"Shushing",L"quiet secret", EmojiCategory::Smileys},{L"🤭",L"Hand over mouth",L"oops giggle", EmojiCategory::Smileys},{L"🫡",L"Salute",L"respect yes", EmojiCategory::Smileys},
        {L"🫠",L"Melting",L"hot embarrassed", EmojiCategory::Smileys},{L"🤖",L"Robot",L"bot ai", EmojiCategory::Smileys},{L"👻",L"Ghost",L"halloween", EmojiCategory::Smileys},
        {L"💀",L"Skull",L"dead", EmojiCategory::Smileys},{L"💩",L"Poop",L"pile", EmojiCategory::Smileys},{L"🤡",L"Clown",L"joke", EmojiCategory::Smileys},
        {L"👍",L"Thumbs up",L"yes good like approve", EmojiCategory::People},{L"👎",L"Thumbs down",L"no dislike", EmojiCategory::People},{L"👏",L"Clapping",L"applause", EmojiCategory::People},
        {L"🙌",L"Raised hands",L"hooray celebrate", EmojiCategory::People},{L"🙏",L"Folded hands",L"pray thanks please", EmojiCategory::People},
        {L"👋",L"Wave",L"hello goodbye", EmojiCategory::People},{L"👌",L"OK hand",L"okay", EmojiCategory::People},{L"✌️",L"Victory",L"peace", EmojiCategory::People},
        {L"🤞",L"Crossed fingers",L"luck", EmojiCategory::People},{L"🤝",L"Handshake",L"deal agree", EmojiCategory::People},{L"💪",L"Flexed biceps",L"strong gym", EmojiCategory::People},
        {L"👀",L"Eyes",L"look watching", EmojiCategory::People},{L"🧠",L"Brain",L"think smart", EmojiCategory::People},
        {L"❤️",L"Red heart",L"love", EmojiCategory::Symbols},{L"🧡",L"Orange heart",L"love", EmojiCategory::Symbols},{L"💛",L"Yellow heart",L"love", EmojiCategory::Symbols},
        {L"💚",L"Green heart",L"love", EmojiCategory::Symbols},{L"💙",L"Blue heart",L"love", EmojiCategory::Symbols},{L"💜",L"Purple heart",L"love", EmojiCategory::Symbols},
        {L"🖤",L"Black heart",L"love", EmojiCategory::Symbols},{L"🤍",L"White heart",L"love", EmojiCategory::Symbols},{L"💔",L"Broken heart",L"sad", EmojiCategory::Symbols},
        {L"💕",L"Two hearts",L"love", EmojiCategory::Symbols},{L"💯",L"Hundred",L"perfect score", EmojiCategory::Symbols},{L"💢",L"Anger",L"mad", EmojiCategory::Symbols},
        {L"🔥",L"Fire",L"hot lit", EmojiCategory::Travel},{L"✨",L"Sparkles",L"magic clean", EmojiCategory::Activities},{L"⭐",L"Star",L"favorite", EmojiCategory::Travel},
        {L"🌟",L"Glowing star",L"bright", EmojiCategory::Travel},{L"🎉",L"Party popper",L"celebration congratulations", EmojiCategory::Activities},
        {L"🎊",L"Confetti",L"celebrate", EmojiCategory::Activities},{L"🎂",L"Birthday cake",L"party", EmojiCategory::Food},{L"🎁",L"Gift",L"present", EmojiCategory::Activities},
        {L"✅",L"Check mark",L"done yes success", EmojiCategory::Symbols},{L"❌",L"Cross mark",L"no error wrong", EmojiCategory::Symbols},{L"⚠️",L"Warning",L"attention", EmojiCategory::Symbols},
        {L"❓",L"Question",L"help", EmojiCategory::Symbols},{L"❗",L"Exclamation",L"important", EmojiCategory::Symbols},{L"🚀",L"Rocket",L"launch ship", EmojiCategory::Travel},
        {L"💡",L"Light bulb",L"idea", EmojiCategory::Objects},{L"📌",L"Pushpin",L"pin", EmojiCategory::Objects},{L"📎",L"Paperclip",L"attachment", EmojiCategory::Objects},
        {L"📝",L"Memo",L"note write", EmojiCategory::Objects},{L"📅",L"Calendar",L"date schedule", EmojiCategory::Objects},{L"📋",L"Clipboard",L"copy", EmojiCategory::Objects},
        {L"🔒",L"Locked",L"secure private", EmojiCategory::Objects},{L"🔑",L"Key",L"password", EmojiCategory::Objects},{L"🔍",L"Magnifier",L"search find", EmojiCategory::Objects},
        {L"⚙️",L"Gear",L"settings", EmojiCategory::Objects},{L"🛠️",L"Tools",L"build repair", EmojiCategory::Objects},{L"💻",L"Laptop",L"computer code", EmojiCategory::Objects},
        {L"📱",L"Phone",L"mobile", EmojiCategory::Objects},{L"📷",L"Camera",L"photo snapshot", EmojiCategory::Objects},{L"🎥",L"Movie camera",L"video", EmojiCategory::Objects},
        {L"🎧",L"Headphones",L"music", EmojiCategory::Objects},{L"🎵",L"Music note",L"song", EmojiCategory::Objects},{L"🎮",L"Game controller",L"gaming", EmojiCategory::Activities},
        {L"☕",L"Coffee",L"tea hot drink", EmojiCategory::Food},{L"🍵",L"Tea",L"green matcha", EmojiCategory::Food},{L"🍕",L"Pizza",L"food", EmojiCategory::Food},
        {L"🍔",L"Burger",L"food", EmojiCategory::Food},{L"🍜",L"Noodles",L"ramen food", EmojiCategory::Food},{L"🍚",L"Rice",L"food", EmojiCategory::Food},
        {L"🍎",L"Apple",L"fruit", EmojiCategory::Food},{L"🍌",L"Banana",L"fruit", EmojiCategory::Food},{L"🥑",L"Avocado",L"fruit", EmojiCategory::Food},
        {L"🐱",L"Cat",L"pet kitten", EmojiCategory::Animals},{L"🐶",L"Dog",L"pet puppy", EmojiCategory::Animals},{L"🐻",L"Bear",L"animal", EmojiCategory::Animals},
        {L"🐼",L"Panda",L"animal", EmojiCategory::Animals},{L"🦊",L"Fox",L"animal", EmojiCategory::Animals},{L"🐸",L"Frog",L"animal", EmojiCategory::Animals},
        {L"🦋",L"Butterfly",L"insect", EmojiCategory::Animals},{L"🌸",L"Cherry blossom",L"flower", EmojiCategory::Animals},{L"🌻",L"Sunflower",L"flower", EmojiCategory::Animals},
        {L"🌱",L"Seedling",L"plant grow", EmojiCategory::Animals},{L"🌳",L"Tree",L"nature", EmojiCategory::Animals},{L"🌍",L"Earth",L"world globe", EmojiCategory::Travel},
        {L"☀️",L"Sun",L"weather sunny", EmojiCategory::Travel},{L"🌙",L"Moon",L"night", EmojiCategory::Travel},{L"🌈",L"Rainbow",L"weather pride", EmojiCategory::Travel},
        {L"⛈️",L"Thunderstorm",L"weather rain", EmojiCategory::Travel},{L"❄️",L"Snowflake",L"cold winter", EmojiCategory::Travel},{L"🌊",L"Wave water",L"ocean sea", EmojiCategory::Travel},
        {L"✈️",L"Airplane",L"flight travel", EmojiCategory::Travel},{L"🚗",L"Car",L"drive", EmojiCategory::Travel},{L"🏠",L"House",L"home", EmojiCategory::Travel},
        {L"🏆",L"Trophy",L"win prize", EmojiCategory::Activities},{L"⚽",L"Football",L"soccer sport", EmojiCategory::Activities},{L"🏀",L"Basketball",L"sport", EmojiCategory::Activities},
        {L"💰",L"Money bag",L"cash currency", EmojiCategory::Objects},{L"💸",L"Flying money",L"spend cash", EmojiCategory::Objects},{L"📈",L"Chart increasing",L"growth up", EmojiCategory::Objects},
        {L"📉",L"Chart decreasing",L"down", EmojiCategory::Objects},{L"⏰",L"Alarm clock",L"time wake", EmojiCategory::Travel},{L"⏳",L"Hourglass",L"wait time", EmojiCategory::Travel},
        {L"🇮🇩",L"Indonesia",L"flag jakarta", EmojiCategory::Flags},{L"🇺🇸",L"United States",L"flag usa america", EmojiCategory::Flags},
        {L"🇬🇧",L"United Kingdom",L"flag uk britain", EmojiCategory::Flags},{L"🇯🇵",L"Japan",L"flag tokyo", EmojiCategory::Flags},
        {L"🇸🇬",L"Singapore",L"flag", EmojiCategory::Flags},{L"🇦🇺",L"Australia",L"flag", EmojiCategory::Flags},
        {L"✉️",L"Envelope",L"email letter mail message", EmojiCategory::Objects},
        {L"🗑️",L"Wastebasket",L"trash bin delete remove rubbish", EmojiCategory::Objects},
        {L"🍞",L"Bread",L"loaf toast food", EmojiCategory::Food},
        {L"💬",L"Speech balloon",L"chat bubble talk comment message", EmojiCategory::Smileys},
        {L"☝️",L"Index pointing up",L"finger hand attention one", EmojiCategory::People},
        {L"↔️",L"Left-right arrow",L"horizontal direction both resize", EmojiCategory::Symbols},
        {L"↕️",L"Up-down arrow",L"vertical direction both resize", EmojiCategory::Symbols},
        {L"💠",L"Diamond with a dot",L"blue geometric jewel", EmojiCategory::Symbols},
        {L"☒",L"Ballot box with X",L"cross checkbox square cancel", EmojiCategory::Symbols},
        {L"@",L"At sign",L"email mention symbol", EmojiCategory::Symbols}
    }; return items;
}
const Json* Json::get(const std::string& key) const { for (auto& [k,v]:members) if(k==key)return &v; return nullptr; }
class Reader {
    const std::string& s; size_t p=0;
    void ws(){while(p<s.size() && (s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))p++;}
    bool str(std::string& out){
        if(p>=s.size()||s[p++]!='"')return false;
        while(p<s.size()) { unsigned char c=s[p++]; if(c=='"')return true; if(c<32)return false;
            if(c=='\\'){if(p>=s.size())return false; char e=s[p++];
                switch(e){case '"':case '\\':case '/':out+=e;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case 'u': { if(p+4>s.size())return false; unsigned x=0; for(int i=0;i<4;i++){char h=s[p++];x*=16;if(h>='0'&&h<='9')x+=h-'0';else if(h>='a'&&h<='f')x+=h-'a'+10;else if(h>='A'&&h<='F')x+=h-'A'+10;else return false;} if(x<128)out+=char(x);else if(x<2048){out+=char(0xc0|(x>>6));out+=char(0x80|(x&63));}else{out+=char(0xe0|(x>>12));out+=char(0x80|((x>>6)&63));out+=char(0x80|(x&63));} break;}
                default:return false;}
            }else out+=c;
        }return false;
    }
    bool value(Json& j,int depth){
        if(depth>16)return false;ws();if(p>=s.size())return false;
        if(s[p]=='"'){j.kind=Json::String;return str(j.text);}
        if(s[p]=='{'||s[p]=='['){bool obj=s[p++]=='{';j.kind=obj?Json::Object:Json::Array;ws();char close=obj?'}':']';if(p<s.size()&&s[p]==close){p++;return true;}
            for(size_t count=0;count<4096;count++){ws();std::string key;if(obj){if(!str(key))return false;ws();if(p>=s.size()||s[p++]!=':')return false;}Json v;if(!value(v,depth+1))return false;j.members.emplace_back(key,std::move(v));ws();if(p>=s.size())return false;char c=s[p++];if(c==close)return true;if(c!=',')return false;}return false;
        }
        for(auto literal:{"true","false","null"}){size_t n=std::char_traits<char>::length(literal);if(s.compare(p,n,literal)==0){j.kind=literal[0]=='n'?Json::Null:Json::Bool;j.text=literal;p+=n;return true;}}
        size_t start=p;if(s[p]=='-')p++;if(p>=s.size())return false;
        if(s[p]=='0')p++;else{if(s[p]<'1'||s[p]>'9')return false;while(p<s.size()&&isdigit(s[p]))p++;}
        if(p<s.size()&&s[p]=='.'){p++;size_t a=p;while(p<s.size()&&isdigit(s[p]))p++;if(a==p)return false;}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')){p++;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))p++;size_t a=p;while(p<s.size()&&isdigit(s[p]))p++;if(a==p)return false;}
        j.kind=Json::Number;j.text=s.substr(start,p-start);return true;
    }
public:Reader(const std::string& input):s(input){} std::optional<Json> run(){Json j;if(!value(j,0))return {};ws();if(p!=s.size())return {};return j;}
};
std::optional<Json> parse_json(const std::string& s){if(s.size()>1024*1024)return {};return Reader(s).run();}
}
