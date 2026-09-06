#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dma_timing {

constexpr uint32_t kPage     = 0x1000;
constexpr int      kTrials   = 51;
constexpr uint32_t kMaxPages = 512;

struct Page {
    uint64_t va = 0;
    uint64_t pa = 0;
};

enum class ApiKind { Scatter, ScatterPage, Scalar, LcScatter };

inline const char* api_name(ApiKind k) {
    switch (k) {
    case ApiKind::Scatter:     return "scatter";
    case ApiKind::ScatterPage: return "scatter_page";
    case ApiKind::Scalar:      return "scalar";
    case ApiKind::LcScatter:   return "lc_scatter";
    }
    return "?";
}

struct Cell {
    ApiKind     api{};
    const char* space = "virtual";
    std::string cache;
    std::string locality;
    uint32_t    pages = 0;
    uint32_t    bpp = 0;
    bool        physical = false;
    bool        force_page = false;
};

struct TrialResult {
    int     ok_pages = 0;
    int64_t prep_ns  = 0;
    int64_t exec_ns  = 0;
    int64_t total_ns = 0;
};

}  // namespace dma_timing
