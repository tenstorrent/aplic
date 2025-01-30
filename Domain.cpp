#include "Aplic.hpp"
#include "Domain.hpp"

using namespace TT_APLIC;

Domain::Domain(const std::shared_ptr<const Aplic>& aplic, std::string_view name, std::shared_ptr<Domain> parent, uint64_t base, uint64_t size, bool is_machine, std::span<const unsigned> hart_indices)
    : aplic_(aplic), name_(name), parent_(parent), base_(base), size_(size), is_machine_(is_machine), hart_indices_(hart_indices.begin(), hart_indices.end())
{
    unsigned num_harts = aplic->numHarts();
    xeip_bits_.resize(num_harts);
    idcs_.resize(num_harts);
    reset();
}

void Domain::reset()
{
    domaincfg_ = Domaincfg{};
    mmsiaddrcfg_ = 0;
    mmsiaddrcfgh_ = Mmsiaddrcfgh{};
    smsiaddrcfg_ = 0;
    smsiaddrcfgh_ = Smsiaddrcfgh{};
    for (unsigned i = 0; i < sourcecfg_.size(); i++) {
        sourcecfg_[i] = Sourcecfg{};
        target_[i] = Target{};
    }
    for (unsigned i = 0; i < setip_.size(); i++) {
        setip_[i] = 0;
        setie_[i] = 0;
    }

    unsigned num_harts = aplic()->numHarts();
    for (unsigned i = 0; i < num_harts; i++)
        xeip_bits_[i] = 0;

    for (unsigned i = 0; i < num_harts; i++)
        idcs_[i] = Idc{};

    for (auto child : children_)
        child->reset();
}

void Domain::updateTopi()
{
    unsigned num_sources = aplic()->numSources();
    for (auto hart_index : hart_indices_) {
        idcs_[hart_index].topi = Topi{};
    }
    for (unsigned i = 1; i <= num_sources; i++) {
        unsigned hart_index = target_[i].hart_index;
        unsigned priority = target_[i].iprio;
        unsigned ithreshold = idcs_[hart_index].ithreshold;
        auto& topi = idcs_[hart_index].topi;
        unsigned topi_prio = topi.priority;
        bool under_threshold = ithreshold == 0 or priority < ithreshold;
        if (under_threshold and pending(i) and enabled(i) and (priority < topi_prio or topi_prio == 0)) {
            topi.priority = priority;
            topi.iid = i;
        }
        topi.legalize();
    }
}

void Domain::inferXeipBits()
{
    for (unsigned i : hart_indices_)
        xeip_bits_[i] = 0;
    if (domaincfg_.ie) {
        for (unsigned hart_index : hart_indices_) {
            if (idcs_[hart_index].iforce)
                xeip_bits_[hart_index] = 1;
        }
        unsigned num_sources = aplic()->numSources();
        for (unsigned i = 1; i <= num_sources; i++) {
            unsigned hart_index = target_[i].hart_index;
            unsigned priority = target_[i].iprio;
            unsigned idelivery = idcs_[hart_index].idelivery;
            unsigned ithreshold = idcs_[hart_index].ithreshold;
            bool under_threshold = ithreshold == 0 or priority < ithreshold;
            //std::cerr << "idelivery: " << idelivery << ", under_threshold: " << under_threshold << ", pending: " << pending(i) << ", enabled: " << enabled(i) << ")\n";
            if (idelivery and under_threshold and pending(i) and enabled(i))
                xeip_bits_[hart_index] = 1;
        }
    }
}

void Domain::runCallbacksAsRequired()
{
    if (domaincfg_.dm == Direct) {
        auto prev_xeip_bits = xeip_bits_;
        inferXeipBits();
        for (unsigned hart_index : hart_indices_) {
            auto xeip_bit = xeip_bits_[hart_index];
            if (prev_xeip_bits[hart_index] != xeip_bit and direct_callback_)
                direct_callback_(hart_index, is_machine_, xeip_bit);
        }
    } else if (aplic()->autoForwardViaMsi) {
        unsigned num_sources = aplic()->numSources();
        for (unsigned i = 0; i <= num_sources; i++) {
            if (readyToForwardViaMsi(i))
                forwardViaMsi(i);
        }
    }
    for (auto child : children_)
        child->runCallbacksAsRequired();
}

uint64_t Domain::msiAddr(unsigned hart_index, unsigned guest_index) const
{
    uint64_t addr = 0;
    auto cfgh = root()->mmsiaddrcfgh_;
    uint64_t g = (hart_index >> cfgh.lhxw) & ((1 << cfgh.hhxw) - 1);
    uint64_t h = hart_index & ((1 << cfgh.lhxw) - 1);
    uint64_t hhxs = cfgh.hhxs;
    if (is_machine_) {
        uint64_t low = root()->mmsiaddrcfg_;
        addr = (uint64_t(cfgh.ppn) << 32) | low;
        addr = (addr | (g << (hhxs + 12)) | (h << cfgh.lhxs)) << 12;
    } else {
        auto scfgh = root()->smsiaddrcfgh_;
        uint64_t low = root()->smsiaddrcfg_;
        addr = (uint64_t(scfgh.ppn) << 32) | low;
        addr = (addr | (g << (hhxs + 12)) | (h << scfgh.lhxs) | guest_index) << 12;
    }
    return addr;
}

bool Domain::rectifiedInputValue(unsigned i) const
{
    if (not sourceIsActive(i))
        return false;
    bool state = aplic()->getSourceState(i);
    switch (sourcecfg_[i].sm) {
        case Detached:
            return false;
        case Edge1:
        case Level1:
            return state;
        case Edge0:
        case Level0:
            return not state;
    }
    assert(false);
}

bool Domain::sourceIsImplemented(unsigned i) const
{
    if (i == 0 or i > aplic()->numSources())
        return false;
    if (parent() and not parent()->sourcecfg_[i].d)
        return false;
    return true;
}

std::shared_ptr<Domain> Domain::root() const { return aplic_.lock()->root(); }
