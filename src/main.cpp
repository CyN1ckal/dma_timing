#include "config.hpp"
#include "page_pool.hpp"
#include "sweep.hpp"
#include "vmm_api.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace dma_timing;

    Config cfg;
    switch (parse_args(argc, argv, cfg)) {
    case ParseStatus::Help:  return 0;
    case ParseStatus::Error: return 1;
    case ParseStatus::Ok:    break;
    }

    _mkdir(cfg.out_dir.c_str());
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SetThreadAffinityMask(GetCurrentThread(), 1);

    VmmApi api;
    std::string err;
    if (!api.load_dlls(cfg.vmm_dir.empty() ? nullptr : cfg.vmm_dir.c_str(), err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    std::vector<LPCSTR> vmm_argv = {
        "",
        "-device",
        cfg.device.c_str(),
        "-norefresh",
        "-waitinitialize",
        "-disable-python",
        "-disable-symbolserver",
        "-disable-symbols",
        "-disable-infodb",
    };
    std::string device_l = cfg.device;
    for (auto& c : device_l) c = static_cast<char>(::tolower(c));
    if (device_l.find("fpga") != std::string::npos) {
        vmm_argv.push_back("-memmap");
        vmm_argv.push_back("auto");
    }

    std::fprintf(stderr, "[init] VMMDLL_Initialize device=%s\n", cfg.device.c_str());
    api.h = api.Initialize(static_cast<DWORD>(vmm_argv.size()), vmm_argv.data());
    if (!api.h) {
        std::fprintf(stderr,
                     "error: VMMDLL_Initialize failed. Check the FPGA/dump path, "
                     "FTD3XX.dll, and that vmm.dll is 64-bit.\n");
        api.close();
        return 1;
    }

    QWORD lc_handle = 0;
    if (api.cfg_get(VMMDLL_OPT_CORE_LEECHCORE_HANDLE, lc_handle) && lc_handle)
        api.hLc = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(lc_handle));

    DWORD pid = 0;
    char name[MAX_PATH]{};
    std::snprintf(name, sizeof(name), "%s", cfg.process.c_str());
    if (!api.PidGetFromName(api.h, name, &pid) || !pid) {
        std::fprintf(stderr, "error: process '%s' not found on target\n", cfg.process.c_str());
        api.close();
        return 1;
    }
    std::fprintf(stderr, "[init] target pid=%lu (%s)\n",
                 static_cast<unsigned long>(pid), cfg.process.c_str());

    PagePool pool;
    if (!pool.build(api, pid, kMaxPages, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        api.close();
        return 1;
    }
    api.flush_mem_and_tlb();

    Sweep sweep;
    if (!sweep.run(api, pid, cfg, pool, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        api.close();
        return 1;
    }

    api.close();
    std::fprintf(stderr, "Fit:  python ..\\scripts\\fit_model.py %s\\summary.csv\n",
                 cfg.out_dir.c_str());
    std::fprintf(stderr, "Plot: python ..\\scripts\\plot_model.py %s\\summary.csv\n",
                 cfg.out_dir.c_str());
    return 0;
}
