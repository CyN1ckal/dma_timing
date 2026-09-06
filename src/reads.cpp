#include "reads.hpp"

namespace dma_timing {
namespace {

constexpr DWORD kReadFlagsBase =
    VMMDLL_FLAG_NOPAGING |
    VMMDLL_FLAG_NOPAGING_IO |
    VMMDLL_FLAG_NOMEMCALLBACK |
    VMMDLL_FLAG_SCATTER_PREPAREEX_NOMEMZERO;

}  // namespace

bool DmaReads::init(VmmApi& api, uint32_t max_pages, std::string& err) {
    data_.assign(static_cast<size_t>(max_pages) * kPage, 0);
    cb_read_.assign(max_pages, 0);
    if (api.LcAllocScatter1 && api.hLc) {
        if (!api.LcAllocScatter1(max_pages, &lc_mems_) || !lc_mems_) {
            err = "LcAllocScatter1 failed";
            return false;
        }
    }
    return true;
}

void DmaReads::close(VmmApi& api) {
    if (hs_virt_) {
        api.Scatter_CloseHandle(hs_virt_);
        hs_virt_ = nullptr;
    }
    if (hs_phys_) {
        api.Scatter_CloseHandle(hs_phys_);
        hs_phys_ = nullptr;
    }
    if (lc_mems_ && api.LcMemFree) {
        api.LcMemFree(lc_mems_);
        lc_mems_ = nullptr;
    }
}

DWORD DmaReads::flags(const std::string& cache, bool force_page) {
    DWORD f = kReadFlagsBase;
    if (cache == "nocache") f |= VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_NOCACHEPUT;
    if (force_page) f |= VMMDLL_FLAG_SCATTER_FORCE_PAGEREAD;
    return f;
}

void DmaReads::prepare_cache(VmmApi& api, const std::string& cache, bool primed) {
    if (cache == "cold")
        api.flush_mem_and_tlb();
    else if ((cache == "warm" || cache == "nocache") && !primed)
        api.flush_mem_and_tlb();
}

VMMDLL_SCATTER_HANDLE DmaReads::handle(VmmApi& api, bool physical, DWORD pid, DWORD flags) {
    VMMDLL_SCATTER_HANDLE& hs = physical ? hs_phys_ : hs_virt_;
    DWORD& stored = physical ? phys_flags_ : virt_flags_;
    const DWORD use_pid = physical ? kPhysicalPid : pid;
    if (hs && stored == flags) return hs;
    if (hs) {
        api.Scatter_CloseHandle(hs);
        hs = nullptr;
    }
    hs = api.Scatter_Initialize(api.h, use_pid, flags);
    stored = flags;
    return hs;
}

TrialResult DmaReads::scatter(VmmApi& api, VMMDLL_SCATTER_HANDLE hs, DWORD pid, DWORD flags,
                              const std::vector<Page>& pool, uint32_t pages, uint32_t bpp,
                              bool physical, const Qpc& qpc) {
    TrialResult r{};
    const int64_t t0 = qpc.ticks();
    api.Scatter_Clear(hs, pid, flags);
    for (uint32_t i = 0; i < pages; ++i) {
        const uint64_t addr = physical ? pool[i].pa : pool[i].va;
        BYTE* dest = data_.data() + static_cast<size_t>(i) * kPage;
        cb_read_[i] = 0;
        api.Scatter_PrepareEx(hs, addr, bpp, dest, &cb_read_[i]);
    }
    const int64_t t1 = qpc.ticks();
    api.Scatter_ExecuteRead(hs);
    const int64_t t2 = qpc.ticks();
    r.prep_ns = qpc.ns(t1 - t0);
    r.exec_ns = qpc.ns(t2 - t1);
    r.total_ns = qpc.ns(t2 - t0);
    for (uint32_t i = 0; i < pages; ++i) {
        if (cb_read_[i] >= bpp) ++r.ok_pages;
    }
    return r;
}

TrialResult DmaReads::scalar(VmmApi& api, DWORD pid, DWORD flags,
                             const std::vector<Page>& pool, uint32_t pages, uint32_t bpp,
                             bool physical, const Qpc& qpc) {
    TrialResult r{};
    const int64_t t0 = qpc.ticks();
    for (uint32_t i = 0; i < pages; ++i) {
        const uint64_t addr = physical ? pool[i].pa : pool[i].va;
        BYTE* dest = data_.data() + static_cast<size_t>(i) * kPage;
        DWORD cb = 0;
        api.MemReadEx(api.h, pid, addr, dest, bpp, &cb, flags);
        cb_read_[i] = cb;
    }
    const int64_t t1 = qpc.ticks();
    r.exec_ns = qpc.ns(t1 - t0);
    r.total_ns = r.exec_ns;
    for (uint32_t i = 0; i < pages; ++i) {
        if (cb_read_[i] >= bpp) ++r.ok_pages;
    }
    return r;
}

TrialResult DmaReads::lc(VmmApi& api, const std::vector<Page>& pool, uint32_t pages,
                         uint32_t bpp, const Qpc& qpc) {
    TrialResult r{};
    for (uint32_t i = 0; i < pages; ++i) {
        MEM_SCATTER* m = lc_mems_[i];
        m->version = MEM_SCATTER_VERSION;
        m->f = FALSE;
        m->qwA = pool[i].pa;
        m->cb = bpp;
        m->iStack = 0;
    }
    const int64_t t0 = qpc.ticks();
    api.LcReadScatter(api.hLc, pages, lc_mems_);
    const int64_t t1 = qpc.ticks();
    r.exec_ns = qpc.ns(t1 - t0);
    r.total_ns = r.exec_ns;
    for (uint32_t i = 0; i < pages; ++i) {
        if (lc_mems_[i]->f) ++r.ok_pages;
    }
    return r;
}

}  // namespace dma_timing
