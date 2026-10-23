#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace mini {

// Empty-query suggestions use the complete launch count, independent of the
// deliberately small usage bonus applied to fuzzy search results.
template<class Apps>
std::vector<std::size_t> frequent_app_indices(const Apps& apps, std::size_t limit,
                                            bool includeUnused = false) {
    std::vector<std::size_t> indices;
    if (limit == 0) return indices;
    indices.reserve(apps.size());
    for (std::size_t i = 0; i < apps.size(); ++i) {
        if (includeUnused || apps[i].uses > 0) indices.push_back(i);
    }
    const auto count = std::min(limit, indices.size());
    std::partial_sort(indices.begin(), indices.begin() + count, indices.end(),
        [&](std::size_t a, std::size_t b) {
            return apps[a].uses != apps[b].uses
                ? apps[a].uses > apps[b].uses : a < b;
        });
    indices.resize(count);
    return indices;
}

} // namespace mini
