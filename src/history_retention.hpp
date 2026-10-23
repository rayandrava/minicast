#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mini {
inline constexpr int normalize_history_retention_days(int days) {
    return days == 14 || days == 30 ? days : 7;
}

inline constexpr std::uint64_t history_ticks_per_day = 86400ULL * 10000000;
inline constexpr std::size_t history_max_items = 500;
inline constexpr std::size_t history_max_bytes = 4 * 1024 * 1024;
inline constexpr std::size_t history_max_item_bytes = 64 * 1024;
// Clipboard input is UTF-16, including its terminator. A BMP code unit can
// expand to three UTF-8 bytes in the encrypted storage representation.
inline constexpr std::size_t history_max_item_characters = history_max_item_bytes / 2 - 1;
inline constexpr std::size_t history_max_item_utf8_bytes = history_max_item_characters * 3;

// T has .text (a basic_string) and .timestamp (Windows FILETIME ticks).
// Keep the existing newest-first order without assuming timestamps are sorted.
template<class T>
bool prune_history(std::vector<T>& items, std::uint64_t now, int days) {
    const auto maximumAge = std::uint64_t(normalize_history_retention_days(days)) * history_ticks_per_day;
    const auto originalSize = items.size();
    std::size_t kept = 0, bytes = 0;
    bool capacityReached = false;
    for (std::size_t i = 0; i < originalSize; ++i) {
        auto& item = items[i];
        // Reject future dates before subtracting; a clock rollback must not
        // underflow and discard unrelated valid entries later in the list.
        if (item.text.empty() || item.timestamp > now || now - item.timestamp > maximumAge) continue;
        if (capacityReached) continue;
        using Character = typename decltype(item.text)::value_type;
        if (kept == history_max_items || item.text.size() > (history_max_bytes - bytes) / sizeof(Character)) {
            capacityReached = true;
            continue;
        }
        bytes += item.text.size() * sizeof(Character);
        if (kept != i) items[kept] = std::move(item);
        ++kept;
    }
    items.resize(kept);
    return kept != originalSize;
}
}
