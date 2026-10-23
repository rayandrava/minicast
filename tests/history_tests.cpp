#include "history_retention.hpp"
#include <iostream>
#include <limits>
#include <string>

using namespace mini;
struct Item { std::u16string text; std::uint64_t timestamp; };
int checks = 0, failures = 0;
void check(bool passed, const char* name) {
    ++checks;
    if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

int main() {
    constexpr auto day = history_ticks_per_day;
    constexpr auto now = 100 * day;
    for (int days : {7, 14, 30}) {
        check(normalize_history_retention_days(days) == days, "supported retention preserved");
        const auto limit = std::uint64_t(days) * day;
        std::vector<Item> items{{u"new", now}, {u"boundary", now - limit},
                                {u"expired", now - limit - 1}, {u"recent after old", now - day}};
        check(prune_history(items, now, days), "expiration reports a change");
        check(items.size() == 3 && items[0].text == u"new" && items[1].text == u"boundary" &&
              items[2].text == u"recent after old", "inclusive age boundary and out-of-order entries");
        check(!prune_history(items, now, days), "repeated pruning without expiration is unchanged");
    }
    for (int invalid : {-100, -1, 0, 1, 8, 13, 15, 29, 31, std::numeric_limits<int>::max()}) {
        check(normalize_history_retention_days(invalid) == 7, "unsupported retention defaults to seven days");
        std::vector<Item> items{{u"eight days old", now - 8 * day}, {u"six days old", now - 6 * day}};
        check(prune_history(items, now, invalid) && items.size() == 1 && items[0].text == u"six days old",
              "invalid preference uses default expiration");
    }

    std::vector<Item> durations{{u"one day", now - day}, {u"ten days", now - 10 * day},
                                {u"twenty days", now - 20 * day}};
    check(!prune_history(durations, now, 30) && durations.size() == 3, "thirty days preserves all eligible entries");
    check(prune_history(durations, now, 14) && durations.size() == 2, "reducing to fourteen days prunes immediately");
    check(!prune_history(durations, now, 30) && durations.size() == 2, "increasing retention cannot restore deleted entries");
    check(prune_history(durations, now, 7) && durations.size() == 1, "reducing to seven days prunes immediately");
    check(prune_history(durations, now + 8 * day, 7) && durations.empty(), "later pruning expires previously retained entry");

    std::vector<Item> future{{u"one tick future", now + 1}, {u"recent", now - day},
                             {u"far future", std::numeric_limits<std::uint64_t>::max()}, {u"old", now - 31 * day}};
    check(prune_history(future, now, 30) && future.size() == 1 && future[0].text == u"recent",
          "future timestamps do not underflow or remove later valid entries");
    std::vector<Item> epoch{{u"first instant", 0}, {u"future", 1}};
    check(prune_history(epoch, 0, 7) && epoch.size() == 1 && epoch[0].timestamp == 0,
          "zero clock value has no cutoff subtraction underflow");
    std::vector<Item> nearLimit{{u"recent", std::numeric_limits<std::uint64_t>::max() - day}};
    check(!prune_history(nearLimit, std::numeric_limits<std::uint64_t>::max(), 7), "large timestamps do not overflow age calculation");
    std::vector<Item> empty;
    check(!prune_history(empty, now, 7), "empty history is unchanged");
    empty.push_back({u"", now});
    check(prune_history(empty, now, 7) && empty.empty(), "empty text entries are removed");

    std::vector<Item> many(history_max_items + 10, {u"entry", now});
    many.front().text = u"first";
    many[history_max_items - 1].text = u"last retained";
    check(prune_history(many, now, 30) && many.size() == history_max_items, "entry count limit enforced");
    check(many.front().text == u"first" && many.back().text == u"last retained", "entry count retains newest prefix");
    check(!prune_history(many, now, 30), "exact count limit is unchanged");
    many.insert(many.begin() + 2, {u"expired", now - 31 * day});
    check(prune_history(many, now, 30) && many.size() == history_max_items && many.back().text == u"last retained",
          "expired entries do not consume count budget");

    const std::u16string full(history_max_bytes / sizeof(char16_t), u'x');
    std::vector<Item> bounded{{full, now}, {u"overflow", now - day}};
    check(prune_history(bounded, now, 30) && bounded.size() == 1 && bounded.front().text.size() == full.size(),
          "four MiB text limit includes exact fit");
    check(!prune_history(bounded, now, 30), "exact byte limit is unchanged");
    std::vector<Item> cumulative{{std::u16string(full.size() / 2, u'a'), now},
                                 {std::u16string(full.size() / 2, u'b'), now}, {u"extra", now}};
    check(prune_history(cumulative, now, 30) && cumulative.size() == 2, "byte limit is cumulative across entries");
    std::vector<Item> expiredLarge{{full, now - 31 * day}, {u"recent", now}};
    check(prune_history(expiredLarge, now, 30) && expiredLarge.size() == 1 && expiredLarge.front().text == u"recent",
          "expired entries do not consume byte budget");
    std::vector<Item> tooLarge{{std::u16string(full.size() + 1, u'x'), now}, {u"older", now}};
    check(prune_history(tooLarge, now, 30) && tooLarge.empty(), "oversized newest entry cannot breach memory cap");
    std::cout << checks << " clipboard history checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
