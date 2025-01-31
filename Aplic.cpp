#include "Aplic.hpp"

using namespace TT_APLIC;

std::shared_ptr<Domain> Aplic::createDomain(
    const std::string& name,
    std::shared_ptr<Domain> parent,
    uint64_t base,
    uint64_t size,
    bool is_machine,
    std::span<const unsigned> hart_indices
) {
    if (base % 0x1000 != 0)
        throw std::runtime_error("base address of domain '" + name + "' (" + std::to_string(base) + ") is not aligned to 4KiB\n");

    if (size < 0x4000)
        throw std::runtime_error("size of domain '" + name + "' (" + std::to_string(size) + ") is less than minimum of 16KiB\n");

    if (size % 4096 != 0)
        throw std::runtime_error("size of domain '" + name + "' (" + std::to_string(size) + ") is not aligned to 4KiB\n");

    for (auto domain : domains_) {
        if (domain->overlaps(base, size))
            throw std::runtime_error("control regions for domains '" + name + "' and '" + domain->name_ + "' overlap\n");
    }

    if (not root_ and parent)
        throw std::runtime_error("first domain created must be root\n");

    if (not parent and not is_machine)
        throw std::runtime_error("root domain must be machine-level\n");

    if (not is_machine and not parent->is_machine_)
        throw std::runtime_error("parent of supervisor-level domain must be machine-level\n");

    if (root_ and not parent)
        throw std::runtime_error("cannot have more than one root domain\n");

    if (hart_indices.size() == 0)
        throw std::runtime_error("domain '" + name + "' must have at least one hart\n");

    for (auto domain : domains_) {
        if (domain->is_machine_ != is_machine)
            continue;
        for (unsigned i : hart_indices) {
            auto it = std::find(domain->hart_indices_.begin(), domain->hart_indices_.end(), i);
            if (it != domain->hart_indices_.end()) {
                std::string privilege = is_machine ? "machine" : "supervisor";
                std::string msg = "hart " + std::to_string(i) + " belongs to multiple " + privilege + "-level domains: '" + name + "' and '" + domain->name_ + "'\n";
                throw std::runtime_error(msg);
            }
        }
    }
    for (unsigned i : hart_indices) {
        if (i >= num_harts_) {
            std::string msg = "for domain '" + name + "', hart index " + std::to_string(i) + " must be less than number of harts, " + std::to_string(num_harts_) + "\n";
            throw std::runtime_error(msg);
        }
    }
    if (not is_machine) {
        for (unsigned i : hart_indices) {
            auto it = std::find(parent->hart_indices_.begin(), parent->hart_indices_.end(), i);
            if (it == parent->hart_indices_.end()) {
                std::string msg = "hart " + std::to_string(i) + " belongs to supervisor-level domain '" + name + "' but not to its machine-level parent domain, '" + parent->name_ + "'\n";
                throw std::runtime_error(msg);
            }
        }
    }

    auto domain = std::make_shared<Domain>(shared_from_this(), name, parent, base, size, is_machine, hart_indices);
    if (parent)
        parent->children_.push_back(domain);
    if (!root_)
        root_ = domain;
    domain->setDirectCallback(direct_callback_);
    domain->setMsiCallback(msi_callback_);
    domains_.push_back(domain);
    return domain;
}

void Aplic::reset()
{
    for (unsigned i = 0; i <= num_sources_; i++)
        source_states_[i] = 0;
    if (root_)
        root_->reset();
}

bool Aplic::containsAddr(uint64_t addr) const {
    if (findDomainByAddr(addr) == nullptr)
        return false;
    return true;
}

bool Aplic::read(uint64_t addr, size_t size, uint32_t& data)
{
    if (size != 4)
        return false;
    if (addr % 4 != 0)
        return false;
    auto domain = findDomainByAddr(addr);
    if (domain == nullptr)
        return false;
    data = domain->read(addr);
    return true;
}

bool Aplic::write(uint64_t addr, size_t size, uint32_t data)
{
    if (size != 4)
        return false;
    if (addr % 4 != 0)
        return false;
    auto domain = findDomainByAddr(addr);
    if (domain == nullptr)
        return false;
    domain->write(addr, data);
    return true;
}

void Aplic::setDirectCallback(DirectDeliveryCallback callback)
{
    direct_callback_ = callback;
    if (root_)
        root_->setDirectCallback(callback);
}

void Aplic::setMsiCallback(MsiDeliveryCallback callback)
{
    msi_callback_ = callback;
    if (root_)
        root_->setMsiCallback(callback);
}

void Aplic::setSourceState(unsigned i, bool state)
{
    assert(i > 0 && i < 1024);
    bool prev_state = source_states_.at(i);
    source_states_[i] = state;
    if (prev_state != state)
        root_->edge(i);
}

bool Aplic::forwardViaMsi(unsigned i)
{
    for (auto domain : domains_) {
        if (domain->readyToForwardViaMsi(i)) {
            domain->forwardViaMsi(i);
            return true;
        }
    }
    return false;
}
