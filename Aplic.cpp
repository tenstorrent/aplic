#include "Aplic.hpp"
#include <unordered_set>

using namespace TT_APLIC;

Aplic::Aplic(unsigned num_harts, unsigned num_sources, std::span<const DomainParams> domain_params_list)
    : num_harts_(num_harts), num_sources_(num_sources)
{
    if (num_harts > 16384)
        throw std::runtime_error("APLIC cannot have more than 16384 harts\n");
    if (num_sources > 1023)
        throw std::runtime_error("APLIC cannot have more than 1023 sources\n");
    source_states_.resize(num_sources_ + 1);

    std::unordered_set<std::string> uniq_names;
    for (const auto& domain_params : domain_params_list) {
        if (uniq_names.contains(domain_params.name))
            throw std::runtime_error("domain name '" + domain_params.name + "' used more than once\n");
        uniq_names.insert(domain_params.name);
    }

    while (domains_.size() < domain_params_list.size()) {
        bool made_progress = false;
        for (const auto& domain_params : domain_params_list) {
            if (findDomainByName(domain_params.name) != nullptr)
                continue; // already created this domain
            std::shared_ptr<Domain> parent = nullptr;
            if (domain_params.parent.has_value()) {
                parent = findDomainByName(domain_params.parent.value());
                if (parent == nullptr)
                    continue; // parent has not been created yet
            }
            size_t child_index = domain_params.child_index.value_or(0);
            if (parent and parent->numChildren() < child_index)
                continue; // haven't reached this child index yet
            if (parent and parent->numChildren() > child_index)
                throw std::runtime_error("domain '" + domain_params.name + "' reuses child index " + std::to_string(child_index) + "\n");
            if (parent == nullptr and domain_params.privilege != Machine)
                throw std::runtime_error("root APLIC domain must be machine mode\n");
            if (parent and parent->privilege() != Machine)
                throw std::runtime_error("domain '" + domain_params.name + "' has a parent domain without machine privilege\n");
            createDomain(domain_params.name, parent, domain_params.base, domain_params.size, domain_params.privilege, domain_params.hart_indices);
            made_progress = true;
        }
        if (not made_progress)
            throw std::runtime_error("invalid domain hierarchy; possible cycle in graph\n");
    }
}

std::shared_ptr<Domain> Aplic::createDomain(
    const std::string& name,
    std::shared_ptr<Domain> parent,
    uint64_t base,
    uint64_t size,
    Privilege privilege,
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

    if (not parent and privilege != Machine)
        throw std::runtime_error("root domain must be machine-level\n");

    if (privilege == Supervisor and parent->privilege_ == Supervisor)
        throw std::runtime_error("parent of supervisor-level domain must be machine-level\n");

    if (root_ and not parent)
        throw std::runtime_error("cannot have more than one root domain\n");

    if (hart_indices.size() == 0)
        throw std::runtime_error("domain '" + name + "' must have at least one hart\n");

    if (findDomainByName(name) != nullptr)
        throw std::runtime_error("domain with name '" + name + "' already exists\n");

    for (auto domain : domains_) {
        if (domain->privilege_ != privilege)
            continue;
        for (unsigned i : hart_indices) {
            auto it = std::find(domain->hart_indices_.begin(), domain->hart_indices_.end(), i);
            if (it != domain->hart_indices_.end()) {
                std::string priv_str = privilege == Machine ? "machine" : "supervisor";
                std::string msg = "hart " + std::to_string(i) + " belongs to multiple " + priv_str + "-level domains: '" + name + "' and '" + domain->name_ + "'\n";
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
    if (privilege == Supervisor) {
        for (unsigned i : hart_indices) {
            auto it = std::find(parent->hart_indices_.begin(), parent->hart_indices_.end(), i);
            if (it == parent->hart_indices_.end()) {
                std::string msg = "hart " + std::to_string(i) + " belongs to supervisor-level domain '" + name + "' but not to its machine-level parent domain, '" + parent->name_ + "'\n";
                throw std::runtime_error(msg);
            }
        }
    }

    auto domain = std::shared_ptr<Domain>(new Domain(this, name, parent, base, size, privilege, hart_indices));
    if (parent)
        parent->children_.push_back(domain);
    if (!root_)
        root_ = domain;
    domain->setDirectCallback(direct_callback_);
    domain->setMsiCallback(msi_callback_);
    domains_.push_back(domain);
    return domain;
}

std::shared_ptr<Domain> Aplic::findDomainByName(std::string_view name) const
{
    for (auto domain : domains_)
        if (domain->name_ == name)
            return domain;
    return nullptr;
}

std::shared_ptr<Domain> Aplic::findDomainByAddr(uint64_t addr) const
{
    for (auto domain : domains_)
        if (domain->containsAddr(addr))
            return domain;
    return nullptr;
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
