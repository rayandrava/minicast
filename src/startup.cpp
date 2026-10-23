#include "startup.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>

namespace mini {
namespace {
constexpr wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RUN_VALUE[] = L"Minicast";
constexpr wchar_t BACKGROUND_ARGUMENT[] = L"\" --background";
// Windows documents a 260-character maximum for Run command lines.
constexpr size_t MAX_RUN_COMMAND = 260;

struct RegistryKey {
    HKEY handle = nullptr;
    ~RegistryKey() { if (handle) RegCloseKey(handle); }
};

std::wstring current_executable() {
    std::wstring path(512, L'\0');
    while (path.size() <= 32768) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!length) return {};
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        if (path.size() == 32768) break;
        path.resize(path.size() * 2);
    }
    return {};
}

std::wstring startup_command() {
    const auto path = current_executable();
    if (path.empty()) return {};
    return L"\"" + path + BACKGROUND_ARGUMENT;
}

bool registered_command(std::wstring& command) {
    std::array<wchar_t, MAX_RUN_COMMAND + 1> buffer{};
    DWORD bytes = static_cast<DWORD>(sizeof(buffer));
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE,
        RRF_RT_REG_SZ | RRF_NOEXPAND | RRF_ZEROONFAILURE, nullptr, buffer.data(), &bytes);
    if (result != ERROR_SUCCESS || bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t)) return false;
    const size_t characters = bytes / sizeof(wchar_t);
    if (characters > buffer.size() || buffer[characters - 1] != L'\0') return false;
    command.assign(buffer.data(), characters - 1);
    return true;
}

bool same_command(const std::wstring& registered, const std::wstring& expected) {
    constexpr size_t suffixLength = (sizeof(BACKGROUND_ARGUMENT) / sizeof(wchar_t)) - 1;
    if (registered.size() != expected.size() || registered.size() <= suffixLength) return false;
    const size_t pathLength = registered.size() - suffixLength;
    // Windows paths are case-insensitive; command arguments remain exact.
    return registered.compare(pathLength, suffixLength, BACKGROUND_ARGUMENT) == 0 &&
        CompareStringOrdinal(registered.data(), static_cast<int>(pathLength),
            expected.data(), static_cast<int>(pathLength), TRUE) == CSTR_EQUAL;
}

bool valid_background_command(const std::wstring& command) {
    constexpr size_t suffixLength = (sizeof(BACKGROUND_ARGUMENT) / sizeof(wchar_t)) - 1;
    if (command.size() <= suffixLength + 1 || command.front() != L'\"' ||
        !command.ends_with(BACKGROUND_ARGUMENT)) return false;
    const auto path = command.substr(1, command.size() - suffixLength - 1);
    if (path.size() < 5 || CompareStringOrdinal(path.data() + path.size() - 4, 4,
        L".exe", 4, TRUE) != CSTR_EQUAL) return false;
    for (wchar_t c : path) if (c < L' ' || c == L'\"') return false;
    const bool drivePath = ((path[0] >= L'A' && path[0] <= L'Z') ||
        (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' &&
        (path[2] == L'\\' || path[2] == L'/');
    bool networkPath = false;
    if (path.starts_with(L"\\\\")) {
        const auto serverEnd = path.find(L'\\', 2);
        const auto shareEnd = serverEnd == path.npos ? path.npos : path.find(L'\\', serverEnd + 1);
        networkPath = serverEnd != path.npos && serverEnd > 2 &&
            shareEnd != path.npos && shareEnd > serverEnd + 1 && shareEnd + 1 < path.size();
    }
    // Do not require the old file to exist: moving a portable copy is exactly
    // when a previously enabled registration needs to follow its new location.
    return drivePath || networkPath;
}

bool report_registry_error(LSTATUS result, std::wstring& error) {
    error = result == ERROR_ACCESS_DENIED
        ? L"Windows blocked the startup setting."
        : L"Could not save the startup setting. Try again.";
    return false;
}
}

bool startup_enabled() {
    const auto expected = startup_command();
    if (expected.empty() || expected.size() > MAX_RUN_COMMAND) return false;
    std::wstring registered;
    return registered_command(registered) && same_command(registered, expected);
}

bool startup_registered() {
    std::wstring registered;
    return registered_command(registered) && valid_background_command(registered);
}

bool set_startup_enabled(bool enabled, std::wstring& error) {
    error.clear();
    RegistryKey key;
    if (!enabled) {
        // Do not create Run merely to remove an absent setting.
        LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &key.handle);
        if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) return true;
        if (result != ERROR_SUCCESS) return report_registry_error(result, error);
        result = RegDeleteValueW(key.handle, RUN_VALUE);
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) return true;
        return report_registry_error(result, error);
    }

    const auto command = startup_command();
    if (command.empty()) {
        error = L"Could not find Minicast's location. Try reopening it.";
        return false;
    }
    if (command.size() > MAX_RUN_COMMAND) {
        error = L"Move Minicast to a shorter folder path, then try again.";
        return false;
    }
    std::wstring registered;
    if (registered_command(registered) && same_command(registered, command)) return true;
    LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key.handle, nullptr);
    if (result != ERROR_SUCCESS) return report_registry_error(result, error);
    result = RegSetValueExW(key.handle, RUN_VALUE, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    if (result != ERROR_SUCCESS) return report_registry_error(result, error);
    if (startup_enabled()) return true;
    error = L"Could not verify the startup setting. Try again.";
    return false;
}
}
