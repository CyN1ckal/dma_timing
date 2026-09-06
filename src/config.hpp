#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dma_timing {

struct Config {
    std::string device  = "fpga";
    std::string process = "explorer.exe";
    std::string out_dir = "results";
    std::string vmm_dir;
};

enum class ParseStatus { Ok, Help, Error };

ParseStatus parse_args(int argc, char** argv, Config& cfg);

// Published page grid, plus MAX_SIZE_RX/4096 ± {1,2,4}.
std::vector<uint32_t> page_grid(uint32_t rx_pages);

}  // namespace dma_timing
