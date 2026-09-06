#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace dma_timing {

struct Qpc {
    int64_t freq = 1;

    Qpc() {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        freq = f.QuadPart;
    }

    int64_t ticks() const {
        LARGE_INTEGER t{};
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }

    int64_t ns(int64_t dt) const { return (dt * 1000000000LL) / freq; }
};

struct Stats {
    int     n = 0;
    int64_t min = 0, p50 = 0, p95 = 0, max = 0;
    double  mean = 0, stdev = 0;
};

inline Stats summarize(std::vector<int64_t> v) {
    Stats s{};
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.n = static_cast<int>(v.size());
    s.min = v.front();
    s.max = v.back();
    s.p50 = v[(v.size() * 50) / 100];
    s.p95 = v[std::min(v.size() - 1, (v.size() * 95) / 100)];
    const double sum = std::accumulate(v.begin(), v.end(), 0.0);
    s.mean = sum / static_cast<double>(v.size());
    double acc = 0;
    for (auto x : v) {
        const double d = static_cast<double>(x) - s.mean;
        acc += d * d;
    }
    s.stdev = (v.size() > 1) ? std::sqrt(acc / static_cast<double>(v.size() - 1)) : 0.0;
    return s;
}

}  // namespace dma_timing
