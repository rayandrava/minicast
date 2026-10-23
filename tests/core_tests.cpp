#include "core.hpp"
#include <iostream>
#include <cmath>
using namespace mini;
int failures=0,checks=0;
void check(bool value,const char* name){checks++;if(!value){std::cerr<<"FAIL: "<<name<<"\n";failures++;}}
void unit_test(const wchar_t* query,long double expected){auto result=convert_units(query);check(result.has_value(),"unit expression recognized");if(result){auto value=wcstold(result->answer.c_str(),nullptr);bool ok=std::abs(value-expected)<std::max(1.L,std::abs(expected))*1e-8L;check(ok,"unit conversion accuracy");if(!ok)std::wcerr<<query<<L" -> "<<result->answer<<L" expected "<<double(expected)<<L" parsed "<<double(value)<<L"\n";}}
int main(){
    const std::pair<const wchar_t*,const wchar_t*> expressions[] = {
        {L"2+3*4",L"14"},{L"(2+3)*4",L"20"},{L"-2^2",L"-4"},{L"(-2)^2",L"4"},
        {L"2^3^2",L"512"},{L"2^-2",L"0.25"},{L"200*15%",L"30"},{L"15% of 200",L"30"},
        {L"sqrt(144)",L"12"},{L"abs(-12.5)",L"12.5"},{L"round(2.5)",L"3"},
        {L"floor(-2.1)",L"-3"},{L"ceil(-2.1)",L"-2"},{L"log(1000)",L"3"},
        {L"ln(e)",L"1"},{L"cos(0)",L"1"},{L"1e3 + 2.5e-1",L"1000.25"},
        {L"= 42",L"42"},{L"12 × 3 ÷ 2",L"18"},{L".5 + .25",L"0.75"},
        {L"0.1+0.2",L"0.3"},{L"-0*1",L"0"}
    };
    for(auto [input,expected]:expressions){auto result=calculate(input);check(result&&!result->error&&result->answer==expected,"calculator precedence and values");if(!result||result->answer!=expected)std::wcerr<<input<<L" -> "<<(result?result->answer:L"no result")<<L"\n";}
    for(auto input:{L"1/0",L"sqrt(-1)",L"log(0)",L"10^9999",L"(-1)^.5"}){auto result=calculate(input);check(result&&result->error,"calculator domain errors");}
    for(auto input:{L"123",L"Chrome",L"2+",L"2 3",L"sqrt()",L"(2+3",L"2+3)",L"1e+",L"1..2",L"1+2 garbage",L"200 usd to idr",L"9am Tokyo to London"})check(!calculate(input),"calculator rejects incomplete and non-arithmetic queries");
    check(!calculate(std::wstring(40,L'(')+L"1"+std::wstring(40,L')')),"calculator recursion bound");
    check(!calculate(std::wstring(257,L'1')),"calculator input bound");
    check(contains_folded(L"INVOICE Number 42",L"invoice"),"clipboard case insensitive search");
    check(!contains_folded(L"small",L"a longer phrase"),"clipboard long query rejection");
    check(contains_folded(L"any",L""),"clipboard empty query");
    for(auto query:{L"code",L"vs code",L"app",L"zz",L"",L"e d"})for(auto name:{L"Visual Studio Code",L"Application 42",L"Edge",L"Notepad",L"Code"})
        check(fuzzy_score(query,name)==fuzzy_score_normalized(lower(query),lower(name)),"normalized scoring parity");
    unit_test(L"25 c to f",77);unit_test(L"32 f to c",0);unit_test(L"0 k to c",-273.15L);
    unit_test(L"1 mi to km",1.609344L);unit_test(L"5 ft 10 in to cm",177.8L);
    unit_test(L"2 gb to mib",1907.3486328125L);unit_test(L"1 lb to g",453.59237L);
    unit_test(L"60 mph to km/h",96.56064L);unit_test(L"1 usgal to l",3.785411784L);
    check(convert_units(L"-1 k to c")->answer==L"Below absolute zero","absolute-zero validation");
    check(convert_units(L"1 kg to km")->answer==L"Incompatible units","dimension validation");
    check(!convert_units(L"hello world"),"non-expression rejected");check(!convert_units(L"nan kg to g"),"nonfinite rejected");
    check(!convert_units(std::wstring(300,L'1')),"input bounded");check(!convert_units(L"1 gal to l"),"ambiguous gallons rejected");
    check(fuzzy_score(L"vs code",L"Visual Studio Code")>0,"fuzzy subsequence");
    check(fuzzy_score(L"code",L"Code")>fuzzy_score(L"code",L"Visual Studio Code"),"exact ranking");
    check(fuzzy_score(L"zzzz",L"Notepad")==0,"no false match");
    auto c=parse_currency(L"100 usd to idr");check(c&&c->from==L"USD"&&c->to==L"IDR","currency parsing");
    struct ImplicitCurrencyExample { const wchar_t* input; const wchar_t* from; const wchar_t* amount; };
    for (auto example : {
        ImplicitCurrencyExample{L"1 usd",L"USD",L"1"},
        {L"1USD",L"USD",L"1"}, {L" $1 ",L"USD",L"1"},
        {L"€1.25",L"EUR",L"1.25"}, {L"£-2.5",L"GBP",L"-2.5"},
        {L"1 euro",L"EUR",L"1"}, {L"2 euros",L"EUR",L"2"},
        {L"1200jpy",L"JPY",L"1200"}, {L"1 yen",L"JPY",L"1"},
        {L".5 us dollars",L"USD",L".5"}, {L"+5 canadian dollar",L"CAD",L"+5"},
        {L"1 singapore dollars",L"SGD",L"1"}, {L"1 ringgit",L"MYR",L"1"},
        {L"1 kwd",L"KWD",L"1"}, {L"1 xcg",L"XCG",L"1"},
        {L"1 zmw",L"ZMW",L"1"}, {L"1 CNY",L"CNY",L"1"},
        {L"1 yuan",L"CNY",L"1"}, {L"1 €",L"EUR",L"1"},
        {L"convert 1 dollar",L"USD",L"1"}
    }) {
        auto result=parse_currency(example.input,L"IDR");
        check(result&&result->from==example.from&&result->to==L"IDR"&&result->amount==example.amount,
              "implicit source currency converts to local currency");
    }
    for (auto input : {L"1 usd",L"1USD",L"$1",L"1 euro"})
        check(!parse_currency(input),"implicit currency requires detected destination");
    for (auto input : {L"1 usd to eur",L"1 usd in euros",L"$1 to eur",L"convert 1usd to EUR"}) {
        auto result=parse_currency(input,L"IDR");
        check(result&&result->from==L"USD"&&result->to==L"EUR",
              "explicit destination overrides local currency");
    }
    auto lowerLocal=parse_currency(L"1 yen",L" idr ");
    check(lowerLocal&&lowerLocal->to==L"IDR","local currency normalized");
    auto sameLocal=parse_currency(L"1 idr",L"IDR");
    check(sameLocal&&sameLocal->from==L"IDR"&&sameLocal->to==L"IDR","same-currency shorthand recognized");
    check(!parse_currency(L"1 usd",L"invalid"),"invalid default currency rejected");
    check(parse_currency(L"1 usd to eur",L"invalid").has_value(),"explicit conversion does not depend on local detection");
    for (auto input : {L"1",L"1 ",L"1 abc",L"1 qqq",L"1 min",L"1 gb",L"1 mib",L"1 kg",L"1 inch",
                       L"1 usd more",L"1 usd to",L"1 usd to ",L"1 usd in",L"1 usd in ",L"1 usd to e",
                       L"1 usd to extra",L"1 usd to eur extra",L"usd",L"$",L"$.",L"-. usd",L"1..2 usd",
                       L"--1 usd",L"nan usd",L"inf usd",L"1e3 usd",L"1000000000000000000 usd"})
        check(!parse_currency(input,L"IDR"),"implicit currency rejects ordinary queries, units, malformed or incomplete expressions");
    check(!parse_currency(std::wstring(129,L'1')+L" usd",L"IDR"),"implicit currency input bounded");
    check(!parse_currency(L"100 usd to idr extra"),"currency trailing garbage");check(!parse_currency(L"--1 usd to idr"),"currency malformed sign");
    check(currency_result(L"0.1",L"0.2",2)==L"0.02","decimal multiplication");
    check(currency_result(L"100",L"16234.5678",2)==L"1623456.78","currency precision");
    check(currency_result(L"1",L"1.005",2)==L"1.01","half-up rounding");
    check(currency_result(L"-1",L"1.005",2)==L"-1.01","negative rounding");
    check(currency_result(L"-0.001",L"1",2)==L"0.00","negative zero");
    check(!currency_result(L"9999999999999",L"1",2),"overflow bounds");
    check(currency_cross_result(L"100",L"0.8",L"16000",2)==L"2000000.00","non-USD cross rate");
    check(currency_cross_result(L"100",L"16000",L"0.8",2)==L"0.01","cross rate small result");
    check(currency_cross_result(L"1",L"3",L"1",6)==L"0.333333","repeating cross rate");
    check(currency_cross_result(L"-1",L"2",L"1.01",2)==L"-0.51","cross negative half rounding");
    check(currency_cross_result(L"0",L"0.8",L"16000",2)==L"0.00","cross zero");
    check(currency_cross_result(L"123.45",L"0.8",L"0.8",2)==L"123.45","equal rate cancellation");
    check(!currency_cross_result(L"1",L"0",L"1",2),"cross zero denominator rejected");
    check(!currency_cross_result(L"1",L"1",L"-1",2),"cross negative rate rejected");
    check(!currency_cross_result(L"9999999999999",L"1",L"1",2),"cross amount precision bound");
    auto json=parse_json(R"({"rate":16234.5,"date":"2026-10-23","extra":[true,false,null]})");
    check(json&&json->get("rate")&&json->get("rate")->text=="16234.5","rate JSON parse");
    check(!parse_json("{\"x\": 1,}"),"trailing comma rejected");
    check(!parse_json("{}garbage"),"trailing data rejected");
    check(!parse_json("{\"x\":01}"),"leading zero rejected");
    check(!parse_json(std::string(20,'[')+std::string(20,']')),"depth bounded");
    check(emojis().size()>100,"emoji catalog present");
    std::vector<int> categoryCounts(emoji_category_count,0);
    bool validCategories=true, uniqueGlyphs=true;
    for(size_t i=0;i<emojis().size();++i){
        const auto category=static_cast<unsigned>(emojis()[i].category);
        if(category>=categoryCounts.size())validCategories=false;
        else ++categoryCounts[category];
        for(size_t j=0;j<i;++j)if(std::wstring_view(emojis()[i].glyph)==emojis()[j].glyph)uniqueGlyphs=false;
    }
    check(validCategories,"every emoji belongs to a valid category");
    check(uniqueGlyphs,"emoji glyphs have unique persistence identities");
    for(int i=0;i<emoji_category_count;++i)
        check(categoryCounts[i]>0&&!emoji_category_name(static_cast<EmojiCategory>(i)).empty(),"every emoji category has a name and entries");
    check(emoji_category_name(EmojiCategory::Count).empty(),"category sentinel has no display name");
    check(emoji_category_name(static_cast<EmojiCategory>(255)).empty(),"out-of-range category has no display name");
    auto categorized=[](std::wstring_view glyph,EmojiCategory category){
        for(const auto& emoji:emojis())if(glyph==emoji.glyph)return emoji.category==category;
        return false;
    };
    check(categorized(L"🤖",EmojiCategory::Smileys),"robot in smileys category");
    check(categorized(L"☝️",EmojiCategory::People),"pointing finger in people category");
    check(categorized(L"🐱",EmojiCategory::Animals),"cat in nature category");
    check(categorized(L"🍞",EmojiCategory::Food),"bread in food category");
    check(categorized(L"✈️",EmojiCategory::Travel),"airplane in travel category");
    check(categorized(L"🎉",EmojiCategory::Activities),"party popper in activities category");
    check(categorized(L"✉️",EmojiCategory::Objects),"envelope in objects category");
    check(categorized(L"↔️",EmojiCategory::Symbols),"direction arrow in symbols category");
    check(categorized(L"🇮🇩",EmojiCategory::Flags),"Indonesia in flags category");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
