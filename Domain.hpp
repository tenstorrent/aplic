#pragma once

#include <functional>
#include <string>
#include <span>
#include <vector>
#include <memory>
#include <cassert>

namespace TT_APLIC {

enum Privilege {
    Machine,
    Supervisor,
};

typedef std::function<bool(unsigned hart_index, Privilege privilege, bool xeip)> DirectDeliveryCallback;
typedef std::function<bool(uint64_t addr, uint32_t data)> MsiDeliveryCallback;

enum SourceMode {
    Inactive,
    Detached,
    Edge1 = 4,
    Edge0,
    Level1,
    Level0,
};

enum DeliveryMode {
    Direct = 0,
    MSI = 1,
};

union Mmsiaddrcfgh {
    uint32_t value = 0;

    void legalize() {
        value &= 0b1001'1111'0111'0111'1111'1111'1111'1111;
    }

    struct {
        unsigned ppn  : 12;  // High part of ppn
        unsigned lhxw : 4;
        unsigned hhxw : 3;
        unsigned res0 : 1;
        unsigned lhxs : 3;
        unsigned res1 : 1;
        unsigned hhxs : 5;
        unsigned res2 : 2;
        unsigned l    : 1;
    } fields;
};

union Smsiaddrcfgh {
    uint32_t value = 0;

    void legalize() {
        value &= 0b0000'0000'0111'0000'0000'1111'1111'1111;
    }

    struct {
        unsigned ppn  : 12;  // High part of ppn
        unsigned res0 : 8;
        unsigned lhxs : 3;
        unsigned res1 : 9;
    } fields;
};

union Target {
    uint32_t value = 0;

    void legalize(Privilege privilege, DeliveryMode dm, std::span<const unsigned> hart_indices) {
        assert(hart_indices.size() > 0);
        if (std::find(hart_indices.begin(), hart_indices.end(), dm0.hart_index) == hart_indices.end())
            dm0.hart_index = hart_indices[0];
        if (dm == Direct) {
            // TODO: only set bits IPRIOLEN-1:0 for iprio
            value &= 0b1111'1111'1111'1100'0000'0000'1111'1111;
            if (dm0.iprio == 0)
                dm0.iprio = 1;
        } else {
            // TODO: add GEILEN parameter? add parameter for width of EIID?
            value &= 0b1111'1111'1111'1111'1111'0111'1111'1111;
            if (privilege == Machine)
              dm1.guest_index = 0;
        }
    }

    // fields for direct delivery mode
    struct {
        unsigned iprio      : 8;
        unsigned res0       : 10;
        unsigned hart_index : 14;
    } dm0;

    // fields for MSI delivery mode
    struct {
        unsigned eiid         : 11;
        unsigned res1         : 1;
        unsigned guest_index  : 6;
        unsigned hart_index   : 14;
    } dm1;
};

union Topi {
    uint32_t value = 0;

    void legalize() {
        value &= 0b0000'0011'1111'1111'0000'0000'1111'1111;
    }

    struct {
        unsigned priority : 8;
        unsigned res0     : 8;
        unsigned iid      : 10;
    } fields;
};

struct Idc {
    uint32_t idelivery = 0;
    uint32_t iforce = 0;
    uint32_t ithreshold = 0;
    Topi topi = Topi{};
};

union Domaincfg {
    uint32_t value = 0x80000000;

    void legalize() {
        value &= 0x00000105;
        value |= 0x80000000;
    }

    struct {
        unsigned be   : 1;
        unsigned res0 : 1;
        unsigned dm   : 1;
        unsigned res1 : 4;
        unsigned bit7 : 1;
        unsigned ie   : 1;
        unsigned res2 : 15;
        unsigned top8 : 8;
    } fields;
};

union Sourcecfg {
    uint32_t value = 0;

    void legalize(unsigned num_children) {
        value &= dx.d ? 0b0111'1111'1111 : 0b0100'0000'0111;
        if (dx.d and num_children == 0)
            value = 0;
        else if (dx.d and d1.child_index >= num_children)
            d1.child_index = 0;
        if (not dx.d and (d0.sm == 2 or d0.sm == 3))
            d0.sm = 0;
    }

    // fields for delegated sources
    struct {
        unsigned child_index  : 10;
        unsigned d            : 1;
    } d1;

    // fields for non-delegated sources
    struct {
        unsigned sm           : 3;
        unsigned res0         : 7;
        unsigned d            : 1;
    } d0;

    // for when d isn't already known
    struct {
        unsigned unknown      : 10;
        unsigned d            : 1;
    } dx;
};

union Genmsi {
    uint32_t value = 0;

    void legalize() {
        value &= 0b1111'1111'1111'1100'0001'0111'1111'1111;
    }

    struct {
        unsigned eiid       : 11;
        unsigned res0       : 1;
        unsigned busy       : 1;
        unsigned res1       : 5;
        unsigned hart_index : 14;
    } fields;
};

class Aplic;

class Domain
{
    friend Aplic;

public:

    const std::string& name() const { return name_; }
    std::shared_ptr<Domain> root() const;
    std::shared_ptr<Domain> parent() const { return parent_.lock(); }

    uint64_t base() const { return base_; }
    uint64_t size() const { return size_; }
    Privilege privilege() const { return privilege_; }
    std::span<const unsigned> hartIndices() const { return hart_indices_; }

    size_t numChildren() const { return children_.size(); }
    std::shared_ptr<Domain> child(unsigned index) { return children_.at(index); }
    std::shared_ptr<const Domain> child(unsigned index) const { return children_.at(index); }

    auto begin()        { return children_.begin(); }
    auto end()          { return children_.end(); }
    auto begin() const  { return children_.begin(); }
    auto end() const    { return children_.end(); }

    bool overlaps(uint64_t base, uint64_t size) const {
        return (base < (base_ + size_)) and (base_ < (base + size));
    }

    bool containsAddr(uint64_t addr) const {
        if (addr >= base_ and addr < base_ + size_)
            return true;
        return false;
    }

    uint32_t readDomaincfg() const { return domaincfg_.value; }

    void writeDomaincfg(uint32_t value) {
        domaincfg_.value = value;
        domaincfg_.legalize();
        if (domaincfg_.fields.dm == Direct)
          genmsi_.value = 0;
        runCallbacksAsRequired();
    }

    uint32_t readSourcecfg(unsigned i) const { return sourcecfg_.at(i).value; }

    void writeSourcecfg(unsigned i, uint32_t value) {
        if (not sourceIsImplemented(i))
            return;
        Sourcecfg new_sourcecfg{value};
        new_sourcecfg.legalize(children_.size());

        auto old_sourcecfg = sourcecfg_[i];

        std::shared_ptr<Domain> new_child = new_sourcecfg.dx.d ? children_[new_sourcecfg.d1.child_index] : nullptr;
        std::shared_ptr<Domain> old_child = old_sourcecfg.dx.d ? children_[old_sourcecfg.d1.child_index] : nullptr;

        if (old_child and new_child != old_child)
            old_child->undelegate(i);

        bool source_was_active = sourceIsActive(i);
        sourcecfg_[i] = new_sourcecfg;
        bool source_is_active = sourceIsActive(i);

        if (not source_is_active) {
            target_[i].value = 0;
            clearIe(i);
            clearIp(i);
        } else if (not source_was_active and domaincfg_.fields.dm == Direct) {
            target_[i].dm0.iprio = 1;
        }

        // source may becoming pending under new source mode
        // TODO: this might have edge cases (suppose DM=1, SM was Level1 and
        // is now Edge1, pending bit was cleared when forwarded by MSI or
        // clripnum; should not set pending bit even though RIV is high)
        if (rectifiedInputValue(i))
            setIp(i);

        runCallbacksAsRequired();
    }

    uint32_t readMmsiaddrcfg() const {
        if (privilege_ != Machine)
            return 0;
        if (parent())
            return root()->mmsiaddrcfg_;
        return mmsiaddrcfg_;
    }

    void writeMmsiaddrcfg(uint32_t value) {
        if (parent())
            return;
        if (mmsiaddrcfgh_.fields.l)
            return;
        mmsiaddrcfg_ = value;
    }

    uint32_t readMmsiaddrcfgh() const {
        if (privilege_ != Machine)
            return 0;
        if (parent()) {
            auto mmsiaddrcfgh = root()->mmsiaddrcfgh_;
            mmsiaddrcfgh.fields.l = 1;
            return mmsiaddrcfgh.value;
        }
        return mmsiaddrcfgh_.value;
    }

    void writeMmsiaddrcfgh(uint32_t value) {
        if (parent())
            return;
        if (mmsiaddrcfgh_.fields.l)
            return;
        mmsiaddrcfgh_.value = value;
        mmsiaddrcfgh_.legalize();
    }

    uint32_t readSmsiaddrcfg() const {
        if (privilege_ != Machine)
            return 0;
        if (parent())
            return root()->smsiaddrcfg_;
        return smsiaddrcfg_;
    }

    void writeSmsiaddrcfg(uint32_t value) {
        if (parent())
            return;
        if (mmsiaddrcfgh_.fields.l)
            return;
        smsiaddrcfg_ = value;
    }

    uint32_t readSmsiaddrcfgh() const {
        if (privilege_ != Machine)
            return 0;
        if (parent())
            return root()->smsiaddrcfgh_.value;
        return smsiaddrcfgh_.value;
    }

    void writeSmsiaddrcfgh(uint32_t value) {
        if (parent())
            return;
        if (mmsiaddrcfgh_.fields.l)
            return;
        smsiaddrcfgh_.value = value;
        smsiaddrcfgh_.legalize();
    }

    uint32_t readSetip(unsigned i) const { return setip_.at(i); }

    void writeSetip(unsigned i, uint32_t value) {
        assert(i < 32);
        for (unsigned j = 0; j < 32; j++) {
            if ((value >> j) & 1)
                trySetIp(i*32+j);
        }
        runCallbacksAsRequired();
    }

    uint32_t readSetipnum() const { return 0; }

    void writeSetipnum(uint32_t value) {
        trySetIp(value);
        runCallbacksAsRequired();
    }

    uint32_t readInClrip(unsigned i) const {
        assert(i < 32);
        uint32_t result = 0;
        for (unsigned j = 0; j < 32; j++) {
            uint32_t bit = uint32_t(rectifiedInputValue(i*32+j));
            result |= bit << j;
        }
        return result;
    }

    void writeInClrip(unsigned i, uint32_t value) {
        assert(i < 32);
        for (unsigned j = 0; j < 32; j++) {
            if ((value >> j) & 1)
                tryClearIp(i*32+j);
        }
        runCallbacksAsRequired();
    }

    uint32_t readClripnum() const { return 0; }

    void writeClripnum(uint32_t value) {
        tryClearIp(value);
        runCallbacksAsRequired();
    }

    uint32_t readSetie(unsigned i) const { return setie_.at(i); }

    void writeSetie(unsigned i, uint32_t value) {
        assert(i < 32);
        for (unsigned j = 0; j < 32; j++) {
            if ((value >> j) & 1)
                setIe(i*32+j);
        }
        runCallbacksAsRequired();
    }

    uint32_t readSetienum() const { return 0; }

    void writeSetienum(uint32_t value) {
        setIe(value);
        runCallbacksAsRequired();
    }

    uint32_t readClrie(unsigned /*i*/) const { return 0; }

    void writeClrie(unsigned i, uint32_t value) {
        assert(i < 32);
        for (unsigned j = 0; j < 32; j++) {
            if ((value >> j) & 1)
                clearIe(i*32+j);
        }
        runCallbacksAsRequired();
    }

    uint32_t readClrienum() const { return 0; }

    void writeClrienum(uint32_t value) {
        clearIe(value);
        runCallbacksAsRequired();
    }

    uint32_t readSetipnumLe() const { return 0; }

    void writeSetipnumLe(uint32_t value) { writeSetipnum(value); }

    uint32_t readSetipnumBe() const { return 0; }

    void writeSetipnumBe(uint32_t value) { writeSetipnum(value); }

    uint32_t readGenmsi() const { return genmsi_.value; }

    void writeGenmsi(uint32_t value) {
        if (domaincfg_.fields.dm == Direct)
            return;
        if (genmsi_.fields.busy)
            return;
        genmsi_.value = value;
        genmsi_.legalize();
        genmsi_.fields.busy = 1;
    }

    uint32_t readTarget(unsigned i) const { return target_.at(i).value; }

    void writeTarget(unsigned i, uint32_t value) {
        if (not sourceIsActive(i))
            return;
        Target target{value};
        target.legalize(privilege_, DeliveryMode(domaincfg_.fields.dm), hart_indices_);
        target_[i] = target;
        updateTopi();
        runCallbacksAsRequired();
    }

    uint32_t readIdelivery(unsigned hart_index) const { return idcs_.at(hart_index).idelivery; }

    void writeIdelivery(unsigned hart_index, uint32_t value) {
        idcs_.at(hart_index).idelivery = value & 1;
        runCallbacksAsRequired();
    }

    uint32_t readIforce(unsigned hart_index) const { return idcs_.at(hart_index).iforce; }

    void writeIforce(unsigned hart_index, uint32_t value) {
        idcs_.at(hart_index).iforce = value & 1;
        runCallbacksAsRequired();
    }

    uint32_t readIthreshold(unsigned hart_index) const { return idcs_.at(hart_index).ithreshold; }

    void writeIthreshold(unsigned hart_index, uint32_t value) {
        idcs_.at(hart_index).ithreshold = value; // TODO: must implement exactly IPRIOLEN bits
        updateTopi();
    }

    uint32_t readTopi(unsigned hart_index) const { return idcs_.at(hart_index).topi.value; }

    void writeTopi(unsigned /*hart_index*/, uint32_t /*value*/) {}

    uint32_t readClaimi(unsigned hart_index) {
        auto topi = idcs_.at(hart_index).topi;
        if (domaincfg_.fields.dm == Direct) {
            auto sm = sourcecfg_[topi.fields.iid].d0.sm;
            if (topi.value == 0)
                idcs_.at(hart_index).iforce = 0;
            else if (sm == Detached or sm == Edge0 or sm == Edge1)
                clearIp(topi.fields.iid);
            runCallbacksAsRequired();
        }
        return topi.value;
    }

    void writeClaimi(unsigned /*hart_index*/, uint32_t /*value*/) {}

private:
    Domain(const Aplic *aplic, std::string_view name, std::shared_ptr<Domain> parent, uint64_t base, uint64_t size, Privilege privilege, std::span<const unsigned> hart_indices);
    Domain(const Domain&) = delete;
    Domain& operator=(const Domain&) = delete;

    bool use_be(uint64_t addr)
    {
        uint64_t offset = addr - base_;
        bool is_setipnum_le = offset == 0x2000;
        bool is_setipnum_be = offset == 0x2004;
        return (domaincfg_.fields.be or is_setipnum_be) and not is_setipnum_le;
    }

    uint32_t read(uint64_t addr)
    {
        uint32_t data = read_le(addr);
        if (use_be(addr))
            data = __builtin_bswap32(data);
        return data;
    }

    uint32_t read_le(uint64_t addr)
    {
        assert(addr % 4 == 0);
        assert(addr >= base_ and addr < base_ + size_);
        uint64_t offset = addr - base_;
        switch (offset) {
            case 0x0000: return readDomaincfg();
            case 0x1bc0: return readMmsiaddrcfg();
            case 0x1bc4: return readMmsiaddrcfgh();
            case 0x1bc8: return readSmsiaddrcfg();
            case 0x1bcc: return readSmsiaddrcfgh();
            case 0x1cdc: return readSetipnum();
            case 0x1ddc: return readClripnum();
            case 0x1edc: return readSetienum();
            case 0x1fdc: return readClrienum();
            case 0x2000: return readSetipnumLe();
            case 0x2004: return readSetipnumBe();
            case 0x3000: return readGenmsi();
        }

        if (offset >= 0x0004 and offset <= 0x0ffc) {
            unsigned i = offset/4;
            return readSourcecfg(i);
        } else if (offset >= 0x1c00 and offset <= 0x1c7c) {
            unsigned i = (offset - 0x1c00)/4;
            return readSetip(i);
        } else if (offset >= 0x1d00 and offset <= 0x1d7c) {
            unsigned i = (offset - 0x1d00)/4;
            return readInClrip(i);
        } else if (offset >= 0x1e00 and offset <= 0x1e7c) {
            unsigned i = (offset - 0x1e00)/4;
            return readSetie(i);
        } else if (offset >= 0x1f00 and offset <= 0x1f7c) {
            unsigned i = (offset - 0x1f00)/4;
            return readClrie(i);
        } else if (offset >= 0x3004 and offset <= 0x3ffc) {
            unsigned i = (offset - 0x3000)/4;
            return readTarget(i);
        } else if (offset >= 0x4000) {
            unsigned hart_index = (offset - 0x4000)/32;
            unsigned idc_offset = (offset - 0x4000) - 32*hart_index;
            if (hart_index >= idcs_.size())
                return 0;
            switch (idc_offset) {
                case 0x00: return readIdelivery(hart_index);
                case 0x04: return readIforce(hart_index);
                case 0x08: return readIthreshold(hart_index);
                case 0x18: return readTopi(hart_index);
                case 0x1c: return readClaimi(hart_index);
            }
        }

        return 0;
    }

    void write(uint64_t addr, uint32_t data)
    {
        if (use_be(addr))
            data = __builtin_bswap32(data);
        write_le(addr, data);
    }

    void write_le(uint64_t addr, uint32_t data)
    {
        assert(addr % 4 == 0);
        assert(addr >= base_ and addr < base_ + size_);
        uint64_t offset = addr - base_;
        switch (offset) {
            case 0x0000: writeDomaincfg(data); return;
            case 0x1bc0: writeMmsiaddrcfg(data); return;
            case 0x1bc4: writeMmsiaddrcfgh(data); return;
            case 0x1bc8: writeSmsiaddrcfg(data); return;
            case 0x1bcc: writeSmsiaddrcfgh(data); return;
            case 0x1cdc: writeSetipnum(data); return;
            case 0x1ddc: writeClripnum(data); return;
            case 0x1edc: writeSetienum(data); return;
            case 0x1fdc: writeClrienum(data); return;
            case 0x2000: writeSetipnumLe(data); return;
            case 0x2004: writeSetipnumBe(data); return;
            case 0x3000: writeGenmsi(data); return;
        }
        if (offset >= 0x0004 and offset <= 0x0ffc) {
            unsigned i = offset/4;
            writeSourcecfg(i, data);
        } else if (offset >= 0x1c00 and offset <= 0x1c7c) {
            unsigned i = (offset - 0x1c00)/4;
            writeSetip(i, data);
        } else if (offset >= 0x1d00 and offset <= 0x1d7c) {
            unsigned i = (offset - 0x1d00)/4;
            writeInClrip(i, data);
        } else if (offset >= 0x1e00 and offset <= 0x1e7c) {
            unsigned i = (offset - 0x1e00)/4;
            writeSetie(i, data);
        } else if (offset >= 0x1f00 and offset <= 0x1f7c) {
            unsigned i = (offset - 0x1f00)/4;
            writeClrie(i, data);
        } else if (offset >= 0x3004 and offset <= 0x3ffc) {
            unsigned i = (offset - 0x3000)/4;
            writeTarget(i, data);
        } else if (offset >= 0x4000) {
            unsigned hart_index = (offset - 0x4000)/32;
            unsigned idc_offset = (offset - 0x4000) - 32*hart_index;
            if (hart_index >= idcs_.size())
                return;
            switch (idc_offset) {
                case 0x00: writeIdelivery(hart_index, data); return;
                case 0x04: writeIforce(hart_index, data); return;
                case 0x08: writeIthreshold(hart_index, data); return;
                case 0x18: writeTopi(hart_index, data); return;
                case 0x1c: writeClaimi(hart_index, data); return;
            }
        }
    }

    void setDirectCallback(DirectDeliveryCallback callback)
    {
        direct_callback_ = callback;
        for (auto child : children_)
            child->setDirectCallback(callback);
    }

    void setMsiCallback(MsiDeliveryCallback callback)
    {
        msi_callback_ = callback;
        for (auto child : children_)
            child->setMsiCallback(callback);
    }

    void reset();

    void edge(unsigned i)
    {
        assert(i > 0 && i < 1024);
        if (sourcecfg_[i].dx.d) {
            children_[sourcecfg_[i].d1.child_index]->edge(i);
            return;
        }
        auto riv = rectifiedInputValue(i);
        auto sm = sourcecfg_[i].d0.sm;
        if (sm == Edge1 or sm == Edge0) {
            if (riv)
                setIp(i);
        } else if (sm == Level1 or sm == Level0) {
            if (riv)
                setIp(i);
            else
                clearIp(i);
        }
        runCallbacksAsRequired();
    }

    void updateTopi();

    void inferXeipBits();

    void runCallbacksAsRequired();

    bool readyToForwardViaMsi(unsigned i) const
    {
        if (domaincfg_.fields.dm != MSI)
            return false;
        if (i == 0)
            return genmsi_.fields.busy;
        return domaincfg_.fields.ie and pending(i) and enabled(i);
    }

    void forwardViaMsi(unsigned i) {
        assert(readyToForwardViaMsi(i));
        if (i == 0) {
            if (msi_callback_) {
                uint64_t addr = msiAddr(genmsi_.fields.hart_index, 0);
                uint32_t data = genmsi_.fields.eiid;
                msi_callback_(addr, data);
            }
            genmsi_.fields.busy = 0;
        } else {
            if (msi_callback_) {
                uint64_t addr = msiAddr(target_[i].dm1.hart_index, target_[i].dm1.guest_index);
                uint32_t data = target_[i].dm1.eiid;
                msi_callback_(addr, data);
            }
            clearIp(i);
        }
    }

    uint64_t msiAddr(unsigned hart_index, unsigned guest_index) const;

    bool rectifiedInputValue(unsigned i) const;

    bool sourceIsImplemented(unsigned i) const;

    bool sourceIsActive(unsigned i) const
    {
        if (i == 0 or i >= 1024)
            return false;
        if (sourcecfg_.at(i).dx.d)
            return false;
        return sourcecfg_.at(i).d0.sm != Inactive;
    }

    void undelegate(unsigned i)
    {
        assert(i > 0 && i < 1024);
        if (sourcecfg_[i].dx.d) {
            auto child = children_[sourcecfg_[i].d1.child_index];
            child->undelegate(i);
        }
        sourcecfg_[i] = Sourcecfg{};
        target_[i] = Target{};
        clearIp(i);
        clearIe(i);
    }

    void trySetIp(unsigned i)
    {
        if (not sourceIsActive(i))
            return;
        switch (sourcecfg_[i].d0.sm) {
            case Detached:
            case Edge0:
            case Edge1:
                setIp(i);
                break;
            case Level0:
            case Level1:
                if (domaincfg_.fields.dm == MSI and rectifiedInputValue(i))
                    setIp(i);
                break;
            default: assert(false);
        }
    }

    void tryClearIp(unsigned i)
    {
        if (not sourceIsActive(i))
            return;
        switch (sourcecfg_[i].d0.sm) {
            case Detached:
            case Edge0:
            case Edge1:
                clearIp(i);
                break;
            case Level0:
            case Level1:
                if (domaincfg_.fields.dm == MSI)
                    clearIp(i);
                break;
            default:
                assert(false);
        }
    }

    void setOrClearIeOrIpBit(bool ie, unsigned i, bool set)
    {
        if (i == 0 or i >= 1024)
            return;
        if (set and not sourceIsActive(i))
            return;
        auto& setix = ie ? setie_ : setip_;
        uint32_t value = setix[i/32];
        uint32_t one_hot = 1 << (i % 32);
        if (set)
            value |= one_hot;
        else
            value &= ~one_hot;
        setix[i/32] = value;
        updateTopi();
    }

    void setIp(unsigned i)   { setOrClearIeOrIpBit(false, i, true); }
    void clearIp(unsigned i) { setOrClearIeOrIpBit(false, i, false); }
    void setIe(unsigned i)   { setOrClearIeOrIpBit(true, i, true); }
    void clearIe(unsigned i) { setOrClearIeOrIpBit(true, i, false); }

    bool enabled(unsigned i) const { return bool((setie_.at(i/32) >> (i % 32)) & 1); }
    bool pending(unsigned i) const { return bool((setip_.at(i/32) >> (i % 32)) & 1); }

    const Aplic * aplic_;
    std::string name_;
    std::weak_ptr<Domain> parent_;
    uint64_t base_;
    uint64_t size_;
    Privilege privilege_;
    std::vector<unsigned> hart_indices_;
    std::vector<std::shared_ptr<Domain>> children_;
    DirectDeliveryCallback direct_callback_ = nullptr;
    MsiDeliveryCallback msi_callback_ = nullptr;
    std::vector<uint8_t> xeip_bits_;

    Domaincfg domaincfg_;
    std::array<Sourcecfg, 1024> sourcecfg_;
    uint32_t     mmsiaddrcfg_;
    Mmsiaddrcfgh mmsiaddrcfgh_;
    uint32_t     smsiaddrcfg_;
    Smsiaddrcfgh smsiaddrcfgh_;
    std::array<uint32_t, 32> setip_;
    std::array<uint32_t, 32> setie_;
    Genmsi genmsi_;
    std::array<Target, 1024> target_;
    std::vector<Idc> idcs_;
};

}
