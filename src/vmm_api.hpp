// Minimal MemProcFS / LeechCore loader.
//
// Dynamically loads vmm.dll + leechcore.dll so readers do not need import
// libraries. Types and option constants match MemProcFS 5.17 / LeechCore 2.23.
// Only the subset used by dma_timing is declared.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

using QWORD = unsigned __int64;

using VMM_HANDLE = struct tdVMM_HANDLE*;
using VMMDLL_SCATTER_HANDLE = HANDLE;

// ---------------------------------------------------------------------------
// LeechCore scatter descriptor (must match leechcore.h)
// ---------------------------------------------------------------------------
#define MEM_SCATTER_VERSION     0xc0fe0002
#define MEM_SCATTER_STACK_SIZE  12

struct MEM_SCATTER {
    DWORD version;
    BOOL  f;
    QWORD qwA;
    union {
        PBYTE pb;
        QWORD _Filler;
    };
    DWORD cb;
    DWORD iStack;
    QWORD vStack[MEM_SCATTER_STACK_SIZE];
};
using PMEM_SCATTER  = MEM_SCATTER*;
using PPMEM_SCATTER = MEM_SCATTER**;

// ---------------------------------------------------------------------------
// MemProcFS flags / options
// ---------------------------------------------------------------------------
constexpr DWORD VMMDLL_FLAG_NOCACHE                      = 0x0001;
constexpr DWORD VMMDLL_FLAG_NOPAGING                     = 0x0010;
constexpr DWORD VMMDLL_FLAG_NOPAGING_IO                  = 0x0020;
constexpr DWORD VMMDLL_FLAG_NOCACHEPUT                   = 0x0100;
constexpr DWORD VMMDLL_FLAG_SCATTER_PREPAREEX_NOMEMZERO  = 0x1000;
constexpr DWORD VMMDLL_FLAG_NOMEMCALLBACK                = 0x2000;
constexpr DWORD VMMDLL_FLAG_SCATTER_FORCE_PAGEREAD       = 0x4000;

constexpr QWORD VMMDLL_OPT_CORE_LEECHCORE_HANDLE         = 0x4000001000000000ULL;
constexpr QWORD VMMDLL_OPT_CONFIG_IS_REFRESH_ENABLED     = 0x2000000300000000ULL;
constexpr QWORD VMMDLL_OPT_CONFIG_VMM_VERSION_MAJOR      = 0x2000000900000000ULL;
constexpr QWORD VMMDLL_OPT_CONFIG_VMM_VERSION_MINOR      = 0x2000000A00000000ULL;
constexpr QWORD VMMDLL_OPT_CONFIG_VMM_VERSION_REVISION   = 0x2000000B00000000ULL;
constexpr QWORD VMMDLL_OPT_WIN_VERSION_MAJOR             = 0x2000010100000000ULL;
constexpr QWORD VMMDLL_OPT_WIN_VERSION_MINOR             = 0x2000010200000000ULL;
constexpr QWORD VMMDLL_OPT_WIN_VERSION_BUILD             = 0x2000010300000000ULL;

constexpr QWORD VMMDLL_OPT_REFRESH_FREQ_MEM              = 0x2001100000000000ULL;
constexpr QWORD VMMDLL_OPT_REFRESH_FREQ_TLB              = 0x2001080000000000ULL;

constexpr QWORD LC_OPT_FPGA_PROBE_MAXPAGES               = 0x0300000100000000ULL;
constexpr QWORD LC_OPT_FPGA_MAX_SIZE_RX                  = 0x0300000300000000ULL;
constexpr QWORD LC_OPT_FPGA_MAX_SIZE_TX                  = 0x0300000400000000ULL;
constexpr QWORD LC_OPT_FPGA_DELAY_READ                   = 0x0300000800000000ULL;
constexpr QWORD LC_OPT_FPGA_FPGA_ID                      = 0x0300008100000000ULL;
constexpr QWORD LC_OPT_FPGA_VERSION_MAJOR                = 0x0300008200000000ULL;
constexpr QWORD LC_OPT_FPGA_VERSION_MINOR                = 0x0300008300000000ULL;
constexpr QWORD LC_OPT_FPGA_ALGO_TINY                    = 0x0300008400000000ULL;

constexpr DWORD VMMDLL_MAP_PTE_VERSION                   = 2;
constexpr DWORD VMMDLL_MAP_MODULE_VERSION                = 6;
constexpr DWORD VMMDLL_MODULE_FLAG_NORMAL                = 0;

constexpr DWORD kPhysicalPid                             = static_cast<DWORD>(-1);

struct VMMDLL_MAP_PTEENTRY {
    QWORD vaBase;
    QWORD cPages;
    QWORD fPage;
    BOOL  fWoW64;
    DWORD _FutureUse1;
    union { LPSTR uszText; LPWSTR wszText; };
    DWORD _Reserved1;
    DWORD cSoftware;
};

struct VMMDLL_MAP_PTE {
    DWORD dwVersion;
    DWORD _Reserved1[5];
    PBYTE pbMultiText;
    DWORD cbMultiText;
    DWORD cMap;
    VMMDLL_MAP_PTEENTRY pMap[1];
};

struct VMMDLL_MAP_MODULEENTRY {
    QWORD vaBase;
    QWORD vaEntry;
    DWORD cbImageSize;
    BOOL  fWoW64;
    union { LPSTR uszText; LPWSTR wszText; };
    DWORD _Reserved3;
    DWORD _Reserved4;
    union { LPSTR uszFullName; LPWSTR wszFullName; };
    DWORD tp;
    DWORD cbFileSizeRaw;
    DWORD cSection;
    DWORD cEAT;
    DWORD cIAT;
    DWORD _Reserved2;
    QWORD _Reserved1[3];
    void* pExDebugInfo;
    void* pExVersionInfo;
};

struct VMMDLL_MAP_MODULE {
    DWORD dwVersion;
    DWORD _Reserved1[5];
    PBYTE pbMultiText;
    DWORD cbMultiText;
    DWORD cMap;
    VMMDLL_MAP_MODULEENTRY pMap[1];
};

// ---------------------------------------------------------------------------
// Function pointer types
// ---------------------------------------------------------------------------
using PFN_VMMDLL_Initialize          = VMM_HANDLE(*)(DWORD, LPCSTR*);
using PFN_VMMDLL_Close               = VOID(*)(VMM_HANDLE);
using PFN_VMMDLL_MemFree             = VOID(*)(PVOID);
using PFN_VMMDLL_ConfigGet           = BOOL(*)(VMM_HANDLE, ULONG64, PULONG64);
using PFN_VMMDLL_ConfigSet           = BOOL(*)(VMM_HANDLE, ULONG64, ULONG64);
using PFN_VMMDLL_PidGetFromName      = BOOL(*)(VMM_HANDLE, LPSTR, PDWORD);
using PFN_VMMDLL_Map_GetPteU         = BOOL(*)(VMM_HANDLE, DWORD, BOOL, VMMDLL_MAP_PTE**);
using PFN_VMMDLL_Map_GetModuleU      = BOOL(*)(VMM_HANDLE, DWORD, VMMDLL_MAP_MODULE**, DWORD);
using PFN_VMMDLL_MemReadEx           = BOOL(*)(VMM_HANDLE, DWORD, ULONG64, PBYTE, DWORD, PDWORD, ULONG64);
using PFN_VMMDLL_MemReadScatter      = DWORD(*)(VMM_HANDLE, DWORD, PPMEM_SCATTER, DWORD, DWORD);
using PFN_VMMDLL_MemVirt2Phys        = BOOL(*)(VMM_HANDLE, DWORD, ULONG64, PULONG64);
using PFN_VMMDLL_Scatter_Initialize  = VMMDLL_SCATTER_HANDLE(*)(VMM_HANDLE, DWORD, DWORD);
using PFN_VMMDLL_Scatter_PrepareEx   = BOOL(*)(VMMDLL_SCATTER_HANDLE, QWORD, DWORD, PBYTE, PDWORD);
using PFN_VMMDLL_Scatter_ExecuteRead = BOOL(*)(VMMDLL_SCATTER_HANDLE);
using PFN_VMMDLL_Scatter_Clear       = BOOL(*)(VMMDLL_SCATTER_HANDLE, DWORD, DWORD);
using PFN_VMMDLL_Scatter_CloseHandle = VOID(*)(VMMDLL_SCATTER_HANDLE);

using PFN_LcReadScatter              = VOID(*)(HANDLE, DWORD, PPMEM_SCATTER);
using PFN_LcAllocScatter1            = BOOL(*)(DWORD, PPMEM_SCATTER*);
using PFN_LcMemFree                  = VOID(*)(PVOID);
using PFN_LcGetOption                = BOOL(*)(HANDLE, QWORD, QWORD*);

struct VmmApi {
    HMODULE vmm{};
    HMODULE lc{};
    VMM_HANDLE h{};
    HANDLE hLc{};  // borrowed from VMM; do not close

    PFN_VMMDLL_Initialize          Initialize{};
    PFN_VMMDLL_Close               Close{};
    PFN_VMMDLL_MemFree             MemFree{};
    PFN_VMMDLL_ConfigGet           ConfigGet{};
    PFN_VMMDLL_ConfigSet           ConfigSet{};
    PFN_VMMDLL_PidGetFromName      PidGetFromName{};
    PFN_VMMDLL_Map_GetPteU         Map_GetPteU{};
    PFN_VMMDLL_Map_GetModuleU      Map_GetModuleU{};
    PFN_VMMDLL_MemReadEx           MemReadEx{};
    PFN_VMMDLL_MemReadScatter      MemReadScatter{};
    PFN_VMMDLL_MemVirt2Phys        MemVirt2Phys{};
    PFN_VMMDLL_Scatter_Initialize  Scatter_Initialize{};
    PFN_VMMDLL_Scatter_PrepareEx   Scatter_PrepareEx{};
    PFN_VMMDLL_Scatter_ExecuteRead Scatter_ExecuteRead{};
    PFN_VMMDLL_Scatter_Clear       Scatter_Clear{};
    PFN_VMMDLL_Scatter_CloseHandle Scatter_CloseHandle{};

    PFN_LcReadScatter              LcReadScatter{};
    PFN_LcAllocScatter1            LcAllocScatter1{};
    PFN_LcMemFree                  LcMemFree{};
    PFN_LcGetOption                LcGetOption{};

    static FARPROC require(HMODULE m, const char* name, std::string& err) {
        FARPROC p = GetProcAddress(m, name);
        if (!p) {
            err = std::string("GetProcAddress failed: ") + name;
        }
        return p;
    }

    bool load_dlls(const char* vmm_dir, std::string& err) {
        auto try_load = [](const char* dir, const char* dll) -> HMODULE {
            if (dir && dir[0]) {
                char path[MAX_PATH];
                std::snprintf(path, sizeof(path), "%s\\%s", dir, dll);
                HMODULE m = LoadLibraryA(path);
                if (m) return m;
            }
            return LoadLibraryA(dll);
        };

        vmm = try_load(vmm_dir, "vmm.dll");
        if (!vmm) {
            err = "failed to load vmm.dll (place MemProcFS files next to the exe, "
                  "on PATH, or pass --vmm-dir)";
            return false;
        }
        lc = try_load(vmm_dir, "leechcore.dll");  // optional; needed for lc_scatter

#define LOAD(fn)                                                          \
        do {                                                              \
            fn = reinterpret_cast<decltype(fn)>(require(vmm, "VMMDLL_" #fn, err)); \
            if (!fn) return false;                                        \
        } while (0)

        LOAD(Initialize);
        LOAD(Close);
        LOAD(MemFree);
        LOAD(ConfigGet);
        LOAD(ConfigSet);
        LOAD(PidGetFromName);
        LOAD(Map_GetPteU);
        LOAD(Map_GetModuleU);
        LOAD(MemReadEx);
        LOAD(MemReadScatter);
        LOAD(MemVirt2Phys);
        LOAD(Scatter_Initialize);
        LOAD(Scatter_PrepareEx);
        LOAD(Scatter_ExecuteRead);
        LOAD(Scatter_Clear);
        LOAD(Scatter_CloseHandle);
#undef LOAD

        if (lc) {
            LcReadScatter   = reinterpret_cast<PFN_LcReadScatter>(GetProcAddress(lc, "LcReadScatter"));
            LcAllocScatter1 = reinterpret_cast<PFN_LcAllocScatter1>(GetProcAddress(lc, "LcAllocScatter1"));
            LcMemFree       = reinterpret_cast<PFN_LcMemFree>(GetProcAddress(lc, "LcMemFree"));
            LcGetOption     = reinterpret_cast<PFN_LcGetOption>(GetProcAddress(lc, "LcGetOption"));
        }
        return true;
    }

    bool cfg_get(QWORD opt, QWORD& value) const {
        ULONG64 v = 0;
        if (!ConfigGet || !h || !ConfigGet(h, opt, &v)) return false;
        value = v;
        return true;
    }

    bool cfg_set(QWORD opt, QWORD value) const {
        return ConfigSet && h && ConfigSet(h, opt, value);
    }

    void flush_mem() const { cfg_set(VMMDLL_OPT_REFRESH_FREQ_MEM, 1); }
    void flush_tlb() const { cfg_set(VMMDLL_OPT_REFRESH_FREQ_TLB, 1); }
    void flush_mem_and_tlb() const {
        flush_mem();
        flush_tlb();
    }

    void close() {
        if (h && Close) {
            Close(h);
            h = nullptr;
        }
        hLc = nullptr;
        if (vmm) { FreeLibrary(vmm); vmm = nullptr; }
        if (lc)  { FreeLibrary(lc);  lc  = nullptr; }
    }
};
