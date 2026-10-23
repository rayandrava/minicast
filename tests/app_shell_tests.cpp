#include "app_shell.hpp"
#include <iostream>
#include <string>

using namespace mini;

int checks = 0;
int failures = 0;
void check(bool passed, const char* name) {
    ++checks;
    if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

int main() {
    struct ShellCase { const wchar_t* input; const wchar_t* expected; const char* name; };
    const ShellCase samples[]{
        {L"", L"", "empty identity stays empty"},
        {L"OpenAI.Codex_2p2nqsd0c76g0!App", L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App",
         "packaged app ID is qualified against AppsFolder"},
        {L"Microsoft.Windows.Explorer", L"shell:AppsFolder\\Microsoft.Windows.Explorer",
         "unpackaged dotted app ID is qualified against AppsFolder"},
        {L"\u30DF\u30CB.\u041F\u0440\u0438\u043B\u043E\u0436\u0435\u043D\u0438\u0435!App",
         L"shell:AppsFolder\\\u30DF\u30CB.\u041F\u0440\u0438\u043B\u043E\u0436\u0435\u043D\u0438\u0435!App",
         "Unicode app ID is preserved while qualifying"},
        {L"C:\\Program Files\\Minicast\\Minicast.exe", L"C:\\Program Files\\Minicast\\Minicast.exe",
         "absolute filesystem path stays unchanged"},
        {L"\\\\server\\Apps\\Editor.lnk", L"\\\\server\\Apps\\Editor.lnk",
         "UNC path stays unchanged"},
        {L"\\\\?\\C:\\Apps\\Editor.exe", L"\\\\?\\C:\\Apps\\Editor.exe",
         "extended device path stays unchanged"},
        {L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App",
         L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App",
         "qualified AppsFolder parsing name is not prefixed again"},
        {L"::{4234d49b-0245-4df3-b780-3893943456e1}\\Microsoft.Windows.Explorer",
         L"::{4234d49b-0245-4df3-b780-3893943456e1}\\Microsoft.Windows.Explorer",
         "CLSID namespace parsing name stays unchanged"},
        {L"shell:Programs", L"shell:Programs", "other qualified shell parsing name stays unchanged"}
    };

    bool idempotent = true;
    bool identitiesPreserved = true;
    for (const auto& sample : samples) {
        std::wstring identity = sample.input;
        const std::wstring original = identity;
        const auto parsingName = app_shell_parsing_name(identity);
        check(parsingName == sample.expected, sample.name);
        idempotent = idempotent && app_shell_parsing_name(parsingName) == parsingName;
        identitiesPreserved = identitiesPreserved && identity == original;
    }
    check(idempotent, "qualifying a parsing name twice is idempotent");
    check(identitiesPreserved, "parsing names do not change stored hotkey and usage identities");

    std::cout << checks << " app shell checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
