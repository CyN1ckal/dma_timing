#include "page_pool.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <unordered_set>

namespace dma_timing {
namespace {

bool probe_va(VmmApi& api, DWORD pid, uint64_t va) {
    BYTE buf[8]{};
    DWORD cb = 0;
    return api.MemReadEx(api.h, pid, va, buf, 8, &cb,
                         VMMDLL_FLAG_NOPAGING | VMMDLL_FLAG_NOPAGING_IO) &&
           cb == 8;
}

bool add_page(VmmApi& api, DWORD pid, uint64_t va,
              std::vector<Page>& out, std::unordered_set<uint64_t>& seen) {
    if (seen.count(va)) return false;
    if (!probe_va(api, pid, va)) return false;
    seen.insert(va);
    Page p{};
    p.va = va;
    uint64_t pa = 0;
    if (api.MemVirt2Phys(api.h, pid, va, &pa) && pa) p.pa = pa;
    out.push_back(p);
    return true;
}

size_t longest_run(const std::vector<Page>& pages, size_t& out_i) {
    if (pages.empty()) {
        out_i = 0;
        return 0;
    }
    size_t best_i = 0, best_n = 1, cur_i = 0, cur_n = 1;
    for (size_t i = 1; i < pages.size(); ++i) {
        if (pages[i].va == pages[i - 1].va + kPage) {
            ++cur_n;
        } else {
            if (cur_n > best_n) {
                best_n = cur_n;
                best_i = cur_i;
            }
            cur_i = i;
            cur_n = 1;
        }
    }
    if (cur_n > best_n) {
        best_n = cur_n;
        best_i = cur_i;
    }
    out_i = best_i;
    return best_n;
}

}  // namespace

bool PagePool::has_physical(const std::string& locality, uint32_t n) const {
    const auto& pool = pick(locality);
    if (pool.size() < n) return false;
    for (uint32_t i = 0; i < n; ++i) {
        if (!pool[i].pa) return false;
    }
    return true;
}

bool PagePool::build(VmmApi& api, DWORD pid, uint32_t want, std::string& err) {
    contig_.clear();
    scatter_.clear();

    std::vector<Page> all;
    std::unordered_set<uint64_t> seen;
    all.reserve(static_cast<size_t>(want) * 8);

    // One resident page per 2 MiB (unique page-directory entry).
    VMMDLL_MAP_PTE* pte = nullptr;
    if (api.Map_GetPteU(api.h, pid, FALSE, &pte) && pte) {
        for (DWORD i = 0; i < pte->cMap && scatter_.size() < want; ++i) {
            const auto& e = pte->pMap[i];
            if (!e.vaBase || !e.cPages) continue;
            for (QWORD off = 0; off < e.cPages && scatter_.size() < want; off += 512) {
                const uint64_t va = e.vaBase + off * kPage;
                const size_t before = all.size();
                if (add_page(api, pid, va, all, seen) && before < all.size())
                    scatter_.push_back(all.back());
            }
        }
        api.MemFree(pte);
    }

    // Dense probe of the largest images for a contiguous VA run.
    VMMDLL_MAP_MODULE* mods = nullptr;
    if (api.Map_GetModuleU(api.h, pid, &mods, VMMDLL_MODULE_FLAG_NORMAL) && mods) {
        const DWORD n = mods->cMap;
        std::vector<int> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return mods->pMap[a].cbImageSize > mods->pMap[b].cbImageSize;
        });
        size_t run_i = 0;
        std::sort(all.begin(), all.end(), [](const Page& a, const Page& b) { return a.va < b.va; });
        size_t run_n = longest_run(all, run_i);
        for (int i : idx) {
            if (run_n >= want) break;
            const auto& m = mods->pMap[i];
            if (!m.vaBase || m.cbImageSize < kPage) continue;
            const uint32_t np = m.cbImageSize / kPage;
            for (uint32_t p = 0; p < np; ++p)
                add_page(api, pid, m.vaBase + static_cast<uint64_t>(p) * kPage, all, seen);
            std::sort(all.begin(), all.end(), [](const Page& a, const Page& b) { return a.va < b.va; });
            run_n = longest_run(all, run_i);
        }
        api.MemFree(mods);
    }

    if (all.empty()) {
        err = "no resident virtual pages could be probed in the target process";
        return false;
    }

    std::sort(all.begin(), all.end(), [](const Page& a, const Page& b) { return a.va < b.va; });
    size_t best_i = 0;
    const size_t best_n = longest_run(all, best_i);
    contig_.assign(all.begin() + static_cast<ptrdiff_t>(best_i),
                   all.begin() + static_cast<ptrdiff_t>(best_i + best_n));

    std::vector<Page> merged = scatter_;
    std::unordered_set<uint64_t> have;
    for (const auto& p : merged) have.insert(p.va >> 21);
    for (const auto& p : all) {
        const uint64_t pd = p.va >> 21;
        if (have.count(pd)) continue;
        have.insert(pd);
        merged.push_back(p);
    }
    std::sort(merged.begin(), merged.end(),
              [](const Page& a, const Page& b) { return a.va < b.va; });
    scatter_.swap(merged);

    std::fprintf(stderr,
                 "[pool] probed %zu resident pages; contiguous run %zu; "
                 "2MiB-strided %zu\n",
                 all.size(), contig_.size(), scatter_.size());
    if (contig_.size() < 8)
        std::fprintf(stderr, "[pool] warning: short contiguous run (%zu pages)\n", contig_.size());
    if (scatter_.size() < 64) {
        std::fprintf(stderr,
                     "[pool] warning: only %zu unique 2MiB regions; scattered "
                     "fits will stop there.\n",
                     scatter_.size());
    }
    return true;
}

}  // namespace dma_timing
