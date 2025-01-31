#pragma once

#include <string>
#include <span>
#include <vector>
#include <memory>
#include <cassert>

#include "Domain.hpp"

namespace TT_APLIC {

class Aplic
{
public:
    Aplic(unsigned num_harts, unsigned num_sources)
        : num_harts_(num_harts), num_sources_(num_sources)
    {
        assert(num_harts <= 16384);
        assert(num_sources < 1024);
        source_states_.resize(num_sources_ + 1);
    }

    std::shared_ptr<Domain> root() const { return root_; }
    unsigned numHarts() const { return num_harts_; }
    unsigned numSources() const { return num_sources_; }

    std::shared_ptr<Domain> createDomain(
        const std::string& name,
        std::shared_ptr<Domain> parent,
        uint64_t base,
        uint64_t size,
        Privilege privilege,
        std::span<const unsigned> hart_indices
    );

    std::shared_ptr<Domain> findDomainByName(std::string_view name) const;

    std::shared_ptr<Domain> findDomainByAddr(uint64_t addr) const;

    void reset();

    bool containsAddr(uint64_t addr) const;

    bool read(uint64_t addr, size_t size, uint32_t& data);

    bool write(uint64_t addr, size_t size, uint32_t data);

    void setDirectCallback(DirectDeliveryCallback callback);

    void setMsiCallback(MsiDeliveryCallback callback);

    bool getSourceState(unsigned i) const { return source_states_.at(i); }

    void setSourceState(unsigned i, bool state);

    bool forwardViaMsi(unsigned i);

    bool autoForwardViaMsi = true;

private:
    unsigned num_harts_;
    unsigned num_sources_;
    std::shared_ptr<Domain> root_;
    std::vector<std::shared_ptr<Domain>> domains_;
    std::vector<bool> source_states_;
    DirectDeliveryCallback direct_callback_ = nullptr;
    MsiDeliveryCallback msi_callback_ = nullptr;
};

}
