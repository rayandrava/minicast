#pragma once
#include <cstdint>
#include <optional>
#include <utility>

namespace mini {
// The caller serializes access. Taking a job never discards the latest value:
// a failed write can be retried on the next save event without a retry loop.
template<class T> class PendingSave {
public:
    struct Job { std::uint64_t revision; T value; };
private:
    T latest{};
    std::uint64_t revision=0;
    bool pending=false;
public:
    std::uint64_t set(T value) { latest=std::move(value);pending=true;return ++revision; }
    std::optional<Job> take() {
        if(!pending)return {};
        pending=false;return Job{revision,latest};
    }
    void retry(const Job& job) {
        // A newer queued value already owns its pending flag. Never replace it
        // with the older snapshot that happened to fail while it was writing.
        if(job.revision==revision)pending=true;
    }
};
}
