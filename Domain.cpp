#include "Aplic.hpp"
#include "Domain.hpp"

using namespace TT_APLIC;

Domain::Domain(const Aplic *aplic, std::string_view name, std::shared_ptr<Domain> parent, uint64_t base, uint64_t size, Privilege privilege, std::span<const unsigned> hart_indices)
    : aplic_(aplic), name_(name), parent_(parent), base_(base), size_(size), privilege_(privilege), hart_indices_(hart_indices.begin(), hart_indices.end())
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

    unsigned num_harts = aplic_->numHarts();
    for (unsigned i = 0; i < num_harts; i++)
        xeip_bits_[i] = 0;

    for (unsigned i = 0; i < num_harts; i++)
        idcs_[i] = Idc{};

    for (auto child : children_)
        child->reset();
}

void Domain::updateTopi()
{
    if (domaincfg_.fields.dm == MSI)
        return;
    unsigned num_sources = aplic_->numSources();
    for (auto hart_index : hart_indices_) {
        idcs_[hart_index].topi = Topi{};
    }
    for (unsigned i = 1; i <= num_sources; i++) {
        unsigned hart_index = target_[i].dm0.hart_index;
        unsigned priority = target_[i].dm0.iprio;
        unsigned ithreshold = idcs_[hart_index].ithreshold;
        auto& topi = idcs_[hart_index].topi;
        unsigned topi_prio = topi.fields.priority;
        bool under_threshold = ithreshold == 0 or priority < ithreshold;
        if (under_threshold and pending(i) and enabled(i) and (priority < topi_prio or topi_prio == 0)) {
            topi.fields.priority = priority;
            topi.fields.iid = i;
        }
        topi.legalize();
    }
}

void Domain::inferXeipBits()
{
    for (unsigned i : hart_indices_)
        xeip_bits_[i] = 0;
    if (domaincfg_.fields.ie) {
        for (unsigned hart_index : hart_indices_) {
            if (idcs_[hart_index].iforce)
                xeip_bits_[hart_index] = 1;
        }
        unsigned num_sources = aplic_->numSources();
        for (unsigned i = 1; i <= num_sources; i++) {
            unsigned hart_index = target_[i].dm0.hart_index;
            unsigned priority = target_[i].dm0.iprio;
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
    if (domaincfg_.fields.dm == Direct) {
        auto prev_xeip_bits = xeip_bits_;
        inferXeipBits();
        for (unsigned hart_index : hart_indices_) {
            auto xeip_bit = xeip_bits_[hart_index];
            if (prev_xeip_bits[hart_index] != xeip_bit and direct_callback_)
                direct_callback_(hart_index, privilege_, xeip_bit);
        }
    } else if (aplic_->autoForwardViaMsi) {
        unsigned num_sources = aplic_->numSources();
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
    uint64_t g = (hart_index >> cfgh.fields.lhxw) & ((1 << cfgh.fields.hhxw) - 1);
    uint64_t h = hart_index & ((1 << cfgh.fields.lhxw) - 1);
    uint64_t hhxs = cfgh.fields.hhxs;
    if (privilege_ == Machine) {
        uint64_t low = root()->mmsiaddrcfg_;
        addr = (uint64_t(cfgh.fields.ppn) << 32) | low;
        addr = (addr | (g << (hhxs + 12)) | (h << cfgh.fields.lhxs)) << 12;
    } else {
        auto scfgh = root()->smsiaddrcfgh_;
        uint64_t low = root()->smsiaddrcfg_;
        addr = (uint64_t(scfgh.fields.ppn) << 32) | low;
        addr = (addr | (g << (hhxs + 12)) | (h << scfgh.fields.lhxs) | guest_index) << 12;
    }
    return addr;
}

bool Domain::rectifiedInputValue(unsigned i) const
{
    if (not sourceIsActive(i))
        return false;
    bool state = aplic_->getSourceState(i);
    switch (sourcecfg_[i].d0.sm) {
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
    if (i == 0 or i > aplic_->numSources())
        return false;
    if (parent() and not parent()->sourcecfg_[i].dx.d)
        return false;
    return true;
}

std::shared_ptr<Domain> Domain::root() const { return aplic_->root(); }
