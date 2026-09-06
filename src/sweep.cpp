#include "sweep.hpp"

#include "reads.hpp"
#include "timing.hpp"
#include "types.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dma_timing {
namespace {

const uint32_t kBpp[] = {8, 16, 32, 64, 128, 256, 512, 768, 1024, 2048, 4096};
const char*    kCaches[] = {"cold", "warm", "nocache"};
const char*    kLocalities[] = {"contiguous", "scattered"};

bool want_scalar(uint32_t pages, uint32_t bpp, const std::string& locality, bool physical) {
    return !physical && locality == "contiguous" && pages <= 128 && (bpp == 8 || bpp == 4096);
}

bool want_force_page(uint32_t pages, uint32_t bpp, bool physical) {
    return !physical && pages <= 128 && (bpp == 8 || bpp == 4096);
}

void write_meta(FILE* f, const Config& cfg, VmmApi& api, DWORD pid,
                size_t n_contig, size_t n_scatter, const Qpc& qpc) {
    QWORD maj = 0, min = 0, rev = 0, win_maj = 0, win_min = 0, win_build = 0;
    QWORD refresh = 0, fpga_id = 0, fpga_maj = 0, fpga_min = 0, tiny = 0;
    QWORD rx = 0, tx = 0, delay = 0, probe = 0;
    api.cfg_get(VMMDLL_OPT_CONFIG_VMM_VERSION_MAJOR, maj);
    api.cfg_get(VMMDLL_OPT_CONFIG_VMM_VERSION_MINOR, min);
    api.cfg_get(VMMDLL_OPT_CONFIG_VMM_VERSION_REVISION, rev);
    api.cfg_get(VMMDLL_OPT_WIN_VERSION_MAJOR, win_maj);
    api.cfg_get(VMMDLL_OPT_WIN_VERSION_MINOR, win_min);
    api.cfg_get(VMMDLL_OPT_WIN_VERSION_BUILD, win_build);
    api.cfg_get(VMMDLL_OPT_CONFIG_IS_REFRESH_ENABLED, refresh);
    api.cfg_get(LC_OPT_FPGA_FPGA_ID, fpga_id);
    api.cfg_get(LC_OPT_FPGA_VERSION_MAJOR, fpga_maj);
    api.cfg_get(LC_OPT_FPGA_VERSION_MINOR, fpga_min);
    api.cfg_get(LC_OPT_FPGA_ALGO_TINY, tiny);
    api.cfg_get(LC_OPT_FPGA_MAX_SIZE_RX, rx);
    api.cfg_get(LC_OPT_FPGA_MAX_SIZE_TX, tx);
    api.cfg_get(LC_OPT_FPGA_DELAY_READ, delay);
    api.cfg_get(LC_OPT_FPGA_PROBE_MAXPAGES, probe);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(f, "dma_timing meta\n");
    std::fprintf(f, "timestamp=%04u-%02u-%02uT%02u:%02u:%02u\n",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::fprintf(f, "device=%s\n", cfg.device.c_str());
    std::fprintf(f, "process=%s\n", cfg.process.c_str());
    std::fprintf(f, "pid=%lu\n", static_cast<unsigned long>(pid));
    std::fprintf(f, "vmm=%llu.%llu.%llu\n",
                 (unsigned long long)maj, (unsigned long long)min, (unsigned long long)rev);
    std::fprintf(f, "target_windows=%llu.%llu.%llu\n",
                 (unsigned long long)win_maj, (unsigned long long)win_min,
                 (unsigned long long)win_build);
    std::fprintf(f, "refresh_enabled=%llu\n", (unsigned long long)refresh);
    std::fprintf(f, "qpc_freq=%lld\n", static_cast<long long>(qpc.freq));
    std::fprintf(f, "trials=%d\n", kTrials);
    std::fprintf(f, "max_pages=%u\n", kMaxPages);
    std::fprintf(f, "pool_contiguous=%zu\n", n_contig);
    std::fprintf(f, "pool_scattered=%zu\n", n_scatter);
    std::fprintf(f, "fpga_rx_pages=%llu\n",
                 rx >= kPage ? (unsigned long long)(rx / kPage) : 0ull);
    std::fprintf(f, "fpga_id=%llu\n", (unsigned long long)fpga_id);
    std::fprintf(f, "fpga_version=%llu.%llu\n",
                 (unsigned long long)fpga_maj, (unsigned long long)fpga_min);
    std::fprintf(f, "fpga_algo_tiny=%llu\n", (unsigned long long)tiny);
    std::fprintf(f, "fpga_max_size_rx=%llu\n", (unsigned long long)rx);
    std::fprintf(f, "fpga_max_size_tx=%llu\n", (unsigned long long)tx);
    std::fprintf(f, "fpga_delay_read_us=%llu\n", (unsigned long long)delay);
    std::fprintf(f, "fpga_probe_maxpages=%llu\n", (unsigned long long)probe);
    std::fprintf(f, "leechcore_handle=%s\n", api.hLc ? "yes" : "no");
}

std::vector<Cell> build_cells(const std::vector<uint32_t>& pages, const PagePool& pool,
                              bool want_lc) {
    std::vector<Cell> cells;
    cells.reserve(4096);
    auto push = [&](ApiKind api, const char* cache, const char* loc, uint32_t n,
                    uint32_t bpp, bool physical, bool force_page) {
        const auto& p = pool.pick(loc);
        if (p.size() < n) return;
        if (physical && !pool.has_physical(loc, n)) return;
        cells.push_back({api, physical ? "physical" : "virtual", cache, loc, n, bpp,
                         physical, force_page});
    };

    for (const char* cache : kCaches) {
        for (const char* loc : kLocalities) {
            for (uint32_t n : pages) {
                for (uint32_t bpp : kBpp) {
                    push(ApiKind::Scatter, cache, loc, n, bpp, false, false);
                    push(ApiKind::Scatter, cache, loc, n, bpp, true, false);
                    if (want_force_page(n, bpp, false))
                        push(ApiKind::ScatterPage, cache, loc, n, bpp, false, true);
                    if (want_scalar(n, bpp, loc, false))
                        push(ApiKind::Scalar, cache, loc, n, bpp, false, false);
                    if (want_lc && std::strcmp(cache, "cold") == 0)
                        push(ApiKind::LcScatter, cache, loc, n, bpp, true, false);
                }
            }
        }
    }
    return cells;
}

}  // namespace

bool Sweep::run(VmmApi& api, DWORD pid, const Config& cfg, const PagePool& pool,
                std::string& err) {
    QWORD rx_bytes = 0;
    uint32_t rx_pages = 0;
    if (api.cfg_get(LC_OPT_FPGA_MAX_SIZE_RX, rx_bytes) && rx_bytes >= kPage)
        rx_pages = static_cast<uint32_t>(rx_bytes / kPage);
    if (rx_pages) {
        std::fprintf(stderr, "[init] FPGA MAX_SIZE_RX=%llu (%u pages); densifying grid\n",
                     (unsigned long long)rx_bytes, rx_pages);
    }

    const auto pages = page_grid(rx_pages);
    const bool want_lc = api.LcReadScatter && api.hLc;

    DmaReads io;
    if (!io.init(api, kMaxPages, err)) return false;

    const std::string trials_path  = cfg.out_dir + "\\trials.csv";
    const std::string summary_path = cfg.out_dir + "\\summary.csv";
    const std::string meta_path    = cfg.out_dir + "\\meta.txt";

    Qpc qpc;
    FILE* fmeta = nullptr;
    fopen_s(&fmeta, meta_path.c_str(), "w");
    if (fmeta) {
        write_meta(fmeta, cfg, api, pid, pool.contiguous().size(), pool.scattered().size(), qpc);
        std::fclose(fmeta);
    }

    FILE* ftrials = nullptr;
    FILE* fsum = nullptr;
    fopen_s(&ftrials, trials_path.c_str(), "w");
    fopen_s(&fsum, summary_path.c_str(), "w");
    if (!ftrials || !fsum) {
        err = "cannot write CSVs in '" + cfg.out_dir + "'";
        if (ftrials) std::fclose(ftrials);
        if (fsum) std::fclose(fsum);
        io.close(api);
        return false;
    }

    std::fprintf(ftrials,
                 "api,space,cache,locality,pages,bytes_per_page,bytes_total,"
                 "trial,ok_pages,prep_ns,exec_ns,total_ns\n");
    std::fprintf(fsum,
                 "api,space,cache,locality,pages,bytes_per_page,bytes_total,"
                 "n_trials,ok_rate,"
                 "prep_ns_p50,"
                 "exec_ns_min,exec_ns_p50,exec_ns_mean,exec_ns_p95,exec_ns_max,exec_ns_stdev,"
                 "total_ns_p50,total_ns_mean,mbps_p50\n");

    const auto cells = build_cells(pages, pool, want_lc);
    std::fprintf(stderr, "[run] %zu cells x %d trials\n", cells.size(), kTrials);

    int cell_i = 0;
    for (const auto& cell : cells) {
        ++cell_i;
        const auto& pages_pool = pool.pick(cell.locality);
        const DWORD flags = DmaReads::flags(cell.cache, cell.force_page);
        const DWORD use_pid = cell.physical ? kPhysicalPid : pid;
        const uint32_t bytes_total = cell.pages * cell.bpp;

        if ((cell_i % 10) == 1 || cell_i == static_cast<int>(cells.size())) {
            std::fprintf(stderr, "[%d/%zu] %s %s %s %s pages=%u bpp=%u\n",
                         cell_i, cells.size(), api_name(cell.api), cell.space,
                         cell.cache.c_str(), cell.locality.c_str(),
                         cell.pages, cell.bpp);
        }

        VMMDLL_SCATTER_HANDLE hs = nullptr;
        if (cell.api == ApiKind::Scatter || cell.api == ApiKind::ScatterPage) {
            hs = io.handle(api, cell.physical, pid, flags);
            if (!hs) {
                std::fprintf(stderr, "  skip: Scatter_Initialize failed\n");
                continue;
            }
        }

        std::vector<int64_t> prep, exec, total;
        prep.reserve(kTrials);
        exec.reserve(kTrials);
        total.reserve(kTrials);
        int ok_sum = 0;

        const bool needs_prime = (cell.cache == "warm" || cell.cache == "nocache") &&
                                 cell.api != ApiKind::LcScatter;
        if (needs_prime) {
            DmaReads::prepare_cache(api, cell.cache, false);
            if (cell.api == ApiKind::Scatter || cell.api == ApiKind::ScatterPage)
                io.scatter(api, hs, use_pid, flags, pages_pool, cell.pages, cell.bpp,
                           cell.physical, qpc);
            else
                io.scalar(api, use_pid, flags, pages_pool, cell.pages, cell.bpp,
                          cell.physical, qpc);
        }

        for (int t = 0; t < kTrials; ++t) {
            DmaReads::prepare_cache(api, cell.cache, needs_prime);
            TrialResult r{};
            switch (cell.api) {
            case ApiKind::Scatter:
            case ApiKind::ScatterPage:
                r = io.scatter(api, hs, use_pid, flags, pages_pool, cell.pages, cell.bpp,
                               cell.physical, qpc);
                break;
            case ApiKind::Scalar:
                r = io.scalar(api, use_pid, flags, pages_pool, cell.pages, cell.bpp,
                              cell.physical, qpc);
                break;
            case ApiKind::LcScatter:
                r = io.lc(api, pages_pool, cell.pages, cell.bpp, qpc);
                break;
            }
            prep.push_back(r.prep_ns);
            exec.push_back(r.exec_ns);
            total.push_back(r.total_ns);
            ok_sum += r.ok_pages;
            std::fprintf(ftrials, "%s,%s,%s,%s,%u,%u,%u,%d,%d,%lld,%lld,%lld\n",
                         api_name(cell.api), cell.space, cell.cache.c_str(),
                         cell.locality.c_str(), cell.pages, cell.bpp, bytes_total, t,
                         r.ok_pages, static_cast<long long>(r.prep_ns),
                         static_cast<long long>(r.exec_ns),
                         static_cast<long long>(r.total_ns));
        }

        const Stats se = summarize(exec);
        const Stats st = summarize(total);
        const Stats sp = summarize(prep);
        const double ok_rate =
            static_cast<double>(ok_sum) / (static_cast<double>(kTrials) * cell.pages);
        const double sec = (se.p50 > 0) ? (static_cast<double>(se.p50) / 1e9) : 0.0;
        const double mbps =
            (sec > 0) ? (static_cast<double>(bytes_total) / sec) / (1024.0 * 1024.0) : 0.0;

        std::fprintf(fsum,
                     "%s,%s,%s,%s,%u,%u,%u,%d,%.4f,"
                     "%lld,"
                     "%lld,%lld,%.1f,%lld,%lld,%.1f,"
                     "%lld,%.1f,%.3f\n",
                     api_name(cell.api), cell.space, cell.cache.c_str(),
                     cell.locality.c_str(), cell.pages, cell.bpp, bytes_total, kTrials,
                     ok_rate, static_cast<long long>(sp.p50),
                     static_cast<long long>(se.min), static_cast<long long>(se.p50),
                     se.mean, static_cast<long long>(se.p95),
                     static_cast<long long>(se.max), se.stdev,
                     static_cast<long long>(st.p50), st.mean, mbps);
        if ((cell_i % 25) == 0) {
            std::fflush(ftrials);
            std::fflush(fsum);
        }
    }

    std::fclose(ftrials);
    std::fclose(fsum);
    io.close(api);

    std::fprintf(stderr, "[done] wrote %s\n", trials_path.c_str());
    std::fprintf(stderr, "[done] wrote %s\n", summary_path.c_str());
    std::fprintf(stderr, "[done] wrote %s\n", meta_path.c_str());
    return true;
}

}  // namespace dma_timing
