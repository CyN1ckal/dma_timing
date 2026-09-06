#pragma once

#include "timing.hpp"
#include "types.hpp"
#include "vmm_api.hpp"

#include <string>
#include <vector>

namespace dma_timing {

class DmaReads {
public:
    bool init(VmmApi& api, uint32_t max_pages, std::string& err);
    void close(VmmApi& api);

    static DWORD flags(const std::string& cache, bool force_page);
    static void prepare_cache(VmmApi& api, const std::string& cache, bool primed);

    VMMDLL_SCATTER_HANDLE handle(VmmApi& api, bool physical, DWORD pid, DWORD flags);

    TrialResult scatter(VmmApi& api, VMMDLL_SCATTER_HANDLE hs, DWORD pid, DWORD flags,
                        const std::vector<Page>& pool, uint32_t pages, uint32_t bpp,
                        bool physical, const Qpc& qpc);
    TrialResult scalar(VmmApi& api, DWORD pid, DWORD flags,
                       const std::vector<Page>& pool, uint32_t pages, uint32_t bpp,
                       bool physical, const Qpc& qpc);
    TrialResult lc(VmmApi& api, const std::vector<Page>& pool, uint32_t pages,
                   uint32_t bpp, const Qpc& qpc);

private:
    std::vector<BYTE>     data_;
    std::vector<DWORD>    cb_read_;
    PPMEM_SCATTER         lc_mems_ = nullptr;
    VMMDLL_SCATTER_HANDLE hs_virt_ = nullptr;
    VMMDLL_SCATTER_HANDLE hs_phys_ = nullptr;
    DWORD                 virt_flags_ = 0;
    DWORD                 phys_flags_ = 0;
};

}  // namespace dma_timing
