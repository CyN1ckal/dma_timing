#pragma once

#include "config.hpp"
#include "page_pool.hpp"
#include "vmm_api.hpp"

#include <string>

namespace dma_timing {

class Sweep {
public:
    // Runs the published grid and writes trials.csv, summary.csv, meta.txt.
    bool run(VmmApi& api, DWORD pid, const Config& cfg, const PagePool& pool,
             std::string& err);
};

}  // namespace dma_timing
