#pragma once

#include <string>
#include <span>
#include <vector>
#include <optional>
#include <memory>
#include <cassert>

#include "Domain.hpp"

namespace TT_APLIC {

struct DomainParams {
    std::string name;
    std::optional<std::string> parent;
    std::optional<size_t> child_index;
    uint64_t base;
    uint64_t size;
    Privilege privilege;
    std::vector<unsigned> hart_indices;
};

class Aplic
{
public:
    Aplic(unsigned num_harts, unsigned num_sources, std::span<const DomainParams> domain_params_list);

    std::shared_ptr<Domain> root() const { return root_; }
    unsigned numHarts() const { return num_harts_; }
    unsigned numSources() const { return num_sources_; }

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
    std::shared_ptr<Domain> createDomain(
        const std::string& name,
        std::shared_ptr<Domain> parent,
        uint64_t base,
        uint64_t size,
        Privilege privilege,
        std::span<const unsigned> hart_indices
    );

    unsigned num_harts_;
    unsigned num_sources_;
    std::shared_ptr<Domain> root_;
    std::vector<std::shared_ptr<Domain>> domains_;
    std::vector<bool> source_states_;
    DirectDeliveryCallback direct_callback_ = nullptr;
    MsiDeliveryCallback msi_callback_ = nullptr;
};

}
