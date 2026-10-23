#include "app_ranking.hpp"
#include <climits>
#include <iostream>
#include <vector>

using namespace mini;
struct App { int uses; };
int failures = 0;
int checks = 0;
void check(bool passed, const char* name) {
    ++checks;
    if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

int main() {
    check(frequent_app_indices(std::vector<App>{}, 5).empty(), "empty application index");
    check(frequent_app_indices(std::vector<App>{{0}, {0}}, 5).empty(), "unused apps never fill suggestions");
    check(frequent_app_indices(std::vector<App>{{1}, {2}}, 0).empty(), "zero result limit");

    const std::vector<App> apps{{0}, {60}, {3}, {1000}, {61}, {1000000}, {200}, {0}};
    const std::vector<std::size_t> topFive{5, 3, 6, 4, 1};
    check(frequent_app_indices(apps, 5) == topFive, "top five use full counts including counts above 60");
    check(frequent_app_indices(apps, 5) == topFive, "repeated queries retain the same ranking");
    check(apps[0].uses == 0 && apps[5].uses == 1000000, "ranking leaves usage untouched");
    check(frequent_app_indices(apps, 100) == std::vector<std::size_t>({5, 3, 6, 4, 1, 2}),
          "large limits preserve used apps without adding unused apps");
    check(frequent_app_indices(apps, 100, true) == std::vector<std::size_t>({5, 3, 6, 4, 1, 2, 0, 7}),
          "applications view retains unused apps after used apps without duplication");
    check(frequent_app_indices(std::vector<App>{{0}, {2}, {0}, {1}}, 5) == std::vector<std::size_t>({1, 3}),
          "small usage history contains only successful launches");
    check(frequent_app_indices(std::vector<App>{{4}, {4}, {4}, {4}, {4}, {4}}, 5) ==
          std::vector<std::size_t>({0, 1, 2, 3, 4}), "count ties retain stable index order at the cutoff");
    check(frequent_app_indices(std::vector<App>{{INT_MAX}, {1}, {INT_MAX - 1}}, 5) ==
          std::vector<std::size_t>({0, 2, 1}), "large counts sort without score arithmetic overflow");

    std::vector<App> large(5000);
    for (int i = 0; i < 5000; ++i) large[i].uses = i;
    check(frequent_app_indices(large, 5) == std::vector<std::size_t>({4999, 4998, 4997, 4996, 4995}),
          "large index returns only the requested most-used apps");
    check(frequent_app_indices(large, 100, true).size() == 100, "applications result construction stays bounded");

    std::cout << checks << " app ranking checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
