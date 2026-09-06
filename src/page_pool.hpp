#pragma once

#include "types.hpp"
#include "vmm_api.hpp"

#include <string>
#include <vector>

namespace dma_timing {

class PagePool {
public:
    bool build(VmmApi& api, DWORD pid, uint32_t want, std::string& err);

    const std::vector<Page>& contiguous() const { return contig_; }
    const std::vector<Page>& scattered() const { return scatter_; }

    const std::vector<Page>& pick(const std::string& locality) const {
        return locality == "scattered" ? scatter_ : contig_;
    }

    bool has_physical(const std::string& locality, uint32_t n) const;

private:
    std::vector<Page> contig_;
    std::vector<Page> scatter_;
};

}  // namespace dma_timing
