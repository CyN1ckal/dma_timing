#include "config.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace dma_timing {
namespace {

void uniquify(std::vector<uint32_t>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    v.erase(std::remove(v.begin(), v.end(), 0u), v.end());
}

}  // namespace

ParseStatus parse_args(int argc, char** argv, Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            std::puts(
                "dma_timing — reproduce the published MemProcFS DMA timing sweep\n"
                "\n"
                "  dma_timing.exe [--device fpga] [--process explorer.exe] [--out results]\n"
                "                 [--vmm-dir <path>]\n"
                "\n"
                "Place vmm.dll, leechcore.dll and FTD3XX.dll next to the exe, on PATH,\n"
                "or in --vmm-dir. Download: https://github.com/ufrisk/MemProcFS/releases\n");
            return ParseStatus::Help;
        }
        if (a == "--device") {
            const char* v = need("--device");
            if (!v) return ParseStatus::Error;
            cfg.device = v;
        } else if (a == "--process") {
            const char* v = need("--process");
            if (!v) return ParseStatus::Error;
            cfg.process = v;
        } else if (a == "--out") {
            const char* v = need("--out");
            if (!v) return ParseStatus::Error;
            cfg.out_dir = v;
        } else if (a == "--vmm-dir") {
            const char* v = need("--vmm-dir");
            if (!v) return ParseStatus::Error;
            cfg.vmm_dir = v;
        } else if (a == "--long") {
            // Published command included --long; it is the only sweep.
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", a.c_str());
            return ParseStatus::Error;
        }
    }
    return ParseStatus::Ok;
}

std::vector<uint32_t> page_grid(uint32_t rx_pages) {
    std::vector<uint32_t> pages = {
        1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 64,
        96, 128, 192, 256, 384, 512,
    };
    if (rx_pages) {
        for (int d : {-4, -2, -1, 0, 1, 2, 4}) {
            const int p = static_cast<int>(rx_pages) + d;
            if (p >= 1 && static_cast<uint32_t>(p) <= kMaxPages)
                pages.push_back(static_cast<uint32_t>(p));
        }
    }
    uniquify(pages);
    return pages;
}

}  // namespace dma_timing
