#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <functional>


namespace TT_APLIC
{

  /// APLIC domain control and status register enumeration
  enum class CsrNumber : unsigned
    {
      Domaincfg,
      Sourcecfg1,
      Sourcecfg1023 = Sourcecfg1 + 1022,
      Mmsiaddrcfg = 0x1bc0 >> 2,
      Mmsiaddrcfgh,
      Smsiaddrcfg,
      Smsiaddrcfgh,
      Setip0 = 0x1c00 >> 2,
      Setip31 = Setip0 + 31,
      Setipnum = 0x1cdc >> 2,
      Inclrip0 = 0x1d00 >> 2,
      Inclrip31 = Inclrip0 + 31,
      Clripnum = 0x1ddc >> 2,
      Setie0 = 0x1e00 >> 2,
      Setie31 = Setie0 + 31,
      Setienum = 0x1edc >> 2,
      Clrie0 = 0x1f00 >> 2,
      Clrie31 = Clrie0 + 31,
      Clrienum = 0x1fdc >> 2,
      Setipnumle = 0x2000 >> 2,
      Setipnumbe,
      Genmsi = 0x3000 >> 2,
      Target1,
      Target1023 = Target1 + 1022
    };


  /// Integer type used to represent a domain CSR value.
  typedef uint32_t CsrValue;

  /// Interrupt source mode.
  enum class SourceMode : unsigned
    {
      Inactive = 0,
      Detached = 1,
      Edge1 = 4,
      Edge0 = 5,
      Level1 = 6,
      Level0 = 7
    };


  /// APLIC Interrupt Delivery Control (IDC).  One per hart. Used
  /// for direct delivery (non message signaled).
  struct Idc
  {
    // Enum corresponding to the rank of the CsrValue items in this struct.
    enum class Field : unsigned { Idelivery, Iforce, Ithreshold, Topi=6, Claimi=7 };

    CsrValue idelivery_= 0;
    CsrValue iforce_ = 0;
    CsrValue ithreshold_ = 0;
    CsrValue topi_ = 0;
  };


  /// Union to pack/unpack the topi field in Idc.
  union IdcTopi
  {
    IdcTopi(CsrValue value)
      : value_(value)
    { }

    CsrValue value_;   // 1st variant

    struct   // 2nd variant
    {
      unsigned prio_ : 8;
      unsigned res0_ : 8;
      unsigned id_   : 10;
      unsigned res1_ : 6;
    } bits_;
  };


  /// Union to pack/unpack the Domaincfg CSR.
  union Domaincfg
  {
    Domaincfg(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits.
    constexpr static CsrValue mask()
    { return 0b0000'0000'0000'0000'0000'0001'1000'0101; }

    CsrValue value_;    // First variant of union

    struct   // Second variant
    {
      unsigned be_    : 1;  // Big endian
      unsigned res0_  : 1;
      unsigned dm_    : 1;  // Deliver mode
      unsigned res1_  : 4;
      unsigned bit7_  : 1;
      unsigned ie_    : 1;  // Interrupt enable
      unsigned res2_  : 15;
      unsigned top8_  : 8;
    } bits_;
  };


  /// Union to pack/unpack the Sourcecfg CSRs
  union Sourcecfg
  {
    Sourcecfg(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits when delegated (D == 1).
    constexpr static CsrValue delegatedMask()
    { return 0b111'1111'1111; }

    /// Mask of writable bits when not delegated (D == 0).
    constexpr static CsrValue nonDelegatedMask()
    { return 0b100'0000'0111; }

    CsrValue value_;  // First variant of union

    struct   // Second variant
    {
      unsigned child_ : 10;    // Child index (relative to parent)
      unsigned d_     : 1;    // Delegate
      unsigned res0_  : 21;
    } bits_;

    struct   // Third variant
    {
      unsigned sm_     : 3;  // Source mode
      unsigned res0_   : 7;
      unsigned d_      : 1;  // Delegate
      unsigned res1_   : 22;
    } bits2_;
  };


  /// Union to pack/unpack the Mmsiaddrcfgh CSR
  union Mmsiaddrcfgh
  {
    Mmsiaddrcfgh(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits.
    constexpr static CsrValue mask()
    { return 0b1001'1111'0111'0111'1111'1111'1111'1111; }

    CsrValue value_; // First union variant.

    struct           // Second variant.
    {
      unsigned ppn_  : 12;  // High part of ppn
      unsigned lhxw_ : 4;
      unsigned hhxw_ : 3;
      unsigned res0_ : 1;
      unsigned lhxs_ : 3;
      unsigned res1_ : 1;
      unsigned hhxs_ : 5;
      unsigned res2_ : 2;
      unsigned l_    : 1;
    } bits_;
  };


  /// Union to pack/unpack the Smsiaddrcfgh CSR
  union Smsiaddrcfgh
  {
    Smsiaddrcfgh(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits.
    constexpr static CsrValue mask()
    { return 0b0000'0000'0111'0000'0000'1111'1111'1111; }

    CsrValue value_;   // First union variant

    struct             // Second variant
    {
      unsigned ppn_  : 12;  // High part of ppn
      unsigned res0_ : 8;
      unsigned lhxs_ : 3;
      unsigned res1_ : 9;
    } bits_;
  };


  /// Union to pack/unpack the genmsi CSR
  union Genmsi
  {
    Genmsi(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits.
    constexpr static CsrValue mask()
    { return 0b1111'1111'1111'1100'0001'0111'1111'1111; }

    CsrValue value_;   // First union variant

    struct             // Second variant
    {
      unsigned eiid_ : 11;  // External interrupt id
      unsigned res0_ : 1;
      unsigned busy_ : 1;
      unsigned res1_ : 5;
      unsigned hart_ : 14;
    } bits_;
  };


  /// Union to pack/unpack the Target CSRs
  union Target
  {
    Target(CsrValue value = 0)
      : value_(value)
    { }

    /// Mask of writable bits.
    constexpr static CsrValue mask()
    { return 0b1111'1111'1111'1100'0000'0000'1111'1111; }

    CsrValue value_;   // First union variant

    struct             // Second variant (non MSI delivery)
    {
      unsigned prio_ : 8;  // Priority
      unsigned res0_ : 10;
      unsigned hart_ : 14;
    } bits_;

    struct             // Third variant (MSI delivery)
    {
      unsigned eiid_  : 11;
      unsigned res1_  : 1;
      unsigned guest_ : 6;
      unsigned mhart_ : 14;
    } mbits_;
  };


  /// Aplic domain control and status register.
  class DomainCsr
  {
  public:

    /// Default constructor.
    DomainCsr() = default;

    DomainCsr(const std::string& name, CsrNumber csrn,
              CsrValue reset, CsrValue mask)
      : name_(name), csrn_(csrn), reset_(reset), value_(reset), mask_(mask)
    { }

    /// Return current value of this CSR.
    CsrValue read() const
    { return value_; }

    /// Set value of this CSR to the given value after masking it with
    /// the associated write mask.
    void write(CsrValue value)
    { value_ = value & mask_; }

    /// Return the name of this CSR.
    std::string name() const
    { return name_; }

    /// Size in bytes of this CSR.
    static unsigned size()
    { return sizeof(value_); }

    /// Offset from the domain address to the address of this CSR.
    unsigned offset() const
    { return unsigned(csrn_) * size(); }

    /// Return the write mask of this CSR.
    CsrValue mask() const
    { return mask_; }

    /// Set the write mask of this CSR.
    void setMask(CsrValue m)
    { mask_ = m; }

    /// Set reset.
    void reset(CsrValue reset)
    { reset_ = reset; }

  protected:

  private:

    std::string name_;
    CsrNumber csrn_ = CsrNumber{0};
    CsrValue reset_ = 0;
    CsrValue value_ = 0;
    CsrValue mask_ = 0;
  };


  /// Model an advanced platform local interrupt controller domain.
  class Domain
  {
  public:

    friend class Aplic;

    /// Aplic domain constants.
    enum { IdcOffset = 0x4000, MaxId = 1023, MaxHart = 16384, Align = 16*1024,
           MaxIpriolen = 8 };

    /// Default constructor.
    Domain()
    { }

    /// Constructor. interruptCount is the largest supported interrupt id and
    /// must be less than or equal to 1023. Size is the number of bytes
    /// occupied by this domain in the memory address space.
    Domain(const std::string& name, std::shared_ptr<Domain> parent, uint64_t addr, uint64_t size,
           unsigned hartCount, unsigned interruptCount, bool isMachine)
      : name_(name), addr_(addr), size_(size), hartCount_(hartCount),
        interruptCount_(interruptCount), parent_(parent),
        isMachine_(isMachine)
    {
      defineCsrs();
      defineIdcs();
      assert(interruptCount <= MaxId);
      assert(hartCount <= MaxHart);
      assert((addr % Align) == 0);  // Address must be a aligned.
      assert((size % Align) == 0);  // Size must be a multiple of alignment.
      assert(size >= IdcOffset + hartCount * 32);
    }

    /// Read a memory mapped register associated with this Domain. Return true
    /// on success. Return false leaving value unmodified if addr is not in the
    /// range of this Domain or if size/alignment is not valid. This method
    /// cannot be const because a read from idc.claimi has a side effect.
    bool read(uint64_t addr, unsigned size, uint64_t& value);

    /// Write a memory mapped register associated with this Domain. Return true
    /// on success. Return false if addr is not in the range of this Domain or if
    /// size/alignment is not valid.
    bool write(uint64_t addr, unsigned size, uint64_t value);

    /// Return a pointer to the child domain or nullptr if this domain has
    /// no child.
    std::shared_ptr<Domain> getChild(unsigned ix) const
    { return ix < children_.size() ? children_.at(ix) : nullptr; }

    /// Return parent of this domain or nullptr if this is the root domain.
    std::shared_ptr<Domain> getParent() const
    { return parent_; }

    /// Return true if given interrupt id is delegated to a child domain.
    /// Return false if id is out of bounds.
    bool isDelegated(unsigned id) const;

    /// Return true if given interrupt id is delegated to a child domain setting
    /// childIx to the index of that child. Return false leaving childIx
    /// unmodified if id is out of bounds or if the interrupt id is not
    /// delegated.
    bool isDelegated(unsigned id, unsigned& childIx) const;

    /// Set the state of the source with the given id.
    bool setSourceState(unsigned id, bool prev, bool state);

    /// Return the source state of the interrupt source with the given id.
    SourceMode sourceMode(unsigned id) const;

    /// Return true if interrupt with given id is active (enabled) in this
    /// domain.
    bool isActive(unsigned id) const
    { return id != 0 and id <= interruptCount_ and not isDelegated(id) and
        sourceMode(id) != SourceMode::Inactive; }

    /// Return true if interrupt with given id is inverted in this domain
    /// (active low).
    bool isInverted(unsigned id) const
    {
      using SM = SourceMode;
      return id != 0 and id <= interruptCount_ and not isDelegated(id) and
        (sourceMode(id)== SM::Edge0 or sourceMode(id) == SM::Level0);
    }

    /// Return true if interrupt with given id is level sensitive this domain.
    bool isLevelSensitive(unsigned id) const
    {
      using SM = SourceMode;
      return id != 0 and id <= interruptCount_ and not isDelegated(id) and
        (sourceMode(id) == SM::Level0 or sourceMode(id) == SM::Level1);
    }

    constexpr bool isFalling(bool prev, bool curr) const
    { return prev and not curr; }

    constexpr bool isRising(bool prev, bool curr) const
    { return not prev and curr; }

    /// Define a callback function for the domain to deliver/un-deliver an
    /// interrupt to a hart. When an interrupt becomes active (ready for
    /// delivery), the domain will call this function which will presumably set
    /// the M/S external interrupt pending bit in the MIP CSR of that hart.
    void setDeliveryMethod(std::function<bool(unsigned hartIx, bool machine, bool ip)> func)
    { deliveryFunc_ = func; }

    /// Define a callback function for the domain to write to a memory location.
    /// This is used to deliver interrupts to the IMSIC by writing to the IMSIC
    /// address.
    void setImsicMethod(std::function<bool(uint64_t addr, unsigned size, uint64_t value)> func)
    { imsicFunc_ = func; }

    /// Return the IMSIC address for the given hart. This is computed from the
    /// MMSIADDRCFG CSRs for machine privilege and from the SMSIADDRCFG CSRS for
    /// supervisor privilege domains. See section 4.9.1 of the riscv spec.
    uint64_t imsicAddress(unsigned hartIx);

    /// Return true if this domain is in big-endian configuration.
    bool bigEndian() const
    { return domaincfg().bits_.be_; }

    /// Return true if interrupts are enabled for this domain.
    bool interruptEnabled() const
    { return domaincfg().bits_.ie_; }

    /// Return true if delivery mode is direct for this mode. Return false if
    /// delivery mode is through MSI.
    bool directDelivery() const
    { return not domaincfg().bits_.dm_; }

    /// Return true if this domain targets machine privilege.
    bool isMachinePrivilege() const
    { return isMachine_; }

    /// Return the memory address corresponding to the given CSR number.
    uint64_t csrAddress(CsrNumber csr) const
    { return addr_ + uint64_t(csr)*sizeof(CsrValue); }

    /// Return the memory address of the IDC structure corresponding to the given hart.
    uint64_t idcAddress(unsigned hart) const
    { return addr_ + IdcOffset + hart*32; }

    /// Return the memory address of the idelivery field of the IDC structure
    /// corresponding to the given hart.
    uint64_t ideliveryAddress(unsigned hart) const
    { return idcAddress(hart); }

    /// Return the memory address of the iforce field of the IDC structure
    /// corresponding to the given hart.
    uint64_t iforceAddress(unsigned hart) const
    { return idcAddress(hart) + sizeof(CsrValue); }

    /// Return the memory address of the ithreshold_ field of the IDC structure
    /// corresponding to the given hart.
    uint64_t ithresholdAddress(unsigned hart) const
    { return idcAddress(hart) + 2*sizeof(CsrValue); }

    /// Return the memory address of the topi field of the IDC structure
    /// corresponding to the given hart.
    uint64_t topiAddress(unsigned hart) const
    { return idcAddress(hart) + 6*sizeof(CsrValue); }

    /// Return the memory address of the claimi field of the IDC structure
    /// corresponding to the given hart.
    uint64_t claimiAddress(unsigned hart) const
    { return idcAddress(hart) + 7*sizeof(CsrValue); }

    /// Advance a csr number by the given amount (add amount to number).
    static CsrNumber advance(CsrNumber csrn, int32_t amount)
    { return CsrNumber(CsrValue(csrn) + amount); }

  protected:

    /// Deliver/undeliver interrupt of given source to the associated hart. This
    /// is called when a source status changes.
    void deliverInterrupt(unsigned id, bool ready);

    /// Add given child to the children of this domain.
    void addChild(std::shared_ptr<Domain> child)
    { children_.push_back(child); }

    /// Return the domaincfg CSR value.
    Domaincfg domaincfg() const
    { return csrs_.at(unsigned(CsrNumber::Domaincfg)).read(); }

    /// Return the pointer to the root domain.
    Domain* rootDomain()
    {
      if (isRoot())
        return this;
      return getParent()->rootDomain();
    }

    /// Helper to read method. Read from the interrupt delivery control section.
    bool readIdc(uint64_t addr, unsigned size, CsrValue& value);

    /// Helper to write method. Write to the interrupt delivery control section.
    bool writeIdc(uint64_t addr, unsigned size, CsrValue value);

    /// Identify the Idc structure and field within the structure corresponding to
    /// the given address returning pointer to the Idc structure and setting field
    /// to the index of the field within it. Return nullptr if address is out
    /// of the Idc structures bound or is unaligned.
    Idc* findIdc(uint64_t addr, uint64_t& idcIndex, Idc::Field& field)
    {
      unsigned fieldSize = sizeof(CsrValue);  // Required size.
      if ((addr & (fieldSize - 1)) != 0)
        return nullptr;

      uint64_t ix = (addr - (addr_ + IdcOffset)) / 32;
      if (ix >= idcs_.size())
        return nullptr;
      idcIndex = ix;

      Idc& idc = idcs_.at(idcIndex);
      size_t idcFieldCount = 32 / sizeof(idc.idelivery_);
      uint64_t itemIx = (addr - (addr_ + IdcOffset)) / fieldSize;
      unsigned idcFieldIx = itemIx % idcFieldCount;
      field = Idc::Field{idcFieldIx};
      return &idc;
    }

    /// Set the interrupt pending bit of the given id. Return true if
    /// successful. Return false if it is not possible to set the bit (see
    /// section 4.7 of the riscv-interrupt spec).
    bool trySetIp(unsigned id);

    /// Clear the interrupt pending bit of the given id. Return true if
    /// successful. Return false if it is not possible to set the bit (see
    /// section 4.7 of the riscv-interrupt spec).
    bool tryClearIp(unsigned id);

    /// Set the interrupt pending bit corresponding to the given interrupt id to
    /// flag. Return true on success and false if id is out of bounds. This has
    /// no effect if the interrupt id is not active in this domain. The top id
    /// for the target host will be updated as a side effect.
    bool setInterruptPending(unsigned id, bool flag);

    /// Set the interrupt enabled bit of the given id. Return true if
    /// successful. Return false if it is not possible to set the bit (see
    /// section 4.7 of the riscv-interrupt spec).
    bool trySetIe(unsigned id);

    /// Clear the interrupt enabled bit of the given id. Return true if
    /// successful. Return false if it is not possible to set the bit (see
    /// section 4.7 of the riscv-interrupt spec).
    bool tryClearIe(unsigned id);

    /// Set the interrupt enabled bit corresponding to the given interrupt id to
    /// flag. Return true on success and false if id is out of bounds. This has
    /// no effect if the interrupt id is not active in this domain. The top id
    /// for the target host will be updated as a side effect.
    bool setInterruptEnabled(unsigned id, bool flag);

    /// Return true if this is domain is a leaf.
    bool isLeaf() const
    { return children_.empty(); }

    /// Return true if this is domain is a root domain.
    bool isRoot() const
    { return not getParent(); }

    /// Return CSR having the given number n.
    DomainCsr& csrAt(CsrNumber n)
    { return csrs_.at(unsigned(n)); }

    /// Define the control and status memory mapped registers associated with
    /// this domain.
    void defineCsrs();

    /// Define the interrupt deliver control structures (one per hart)
    /// associated with this domain. IDC is used only in direct (non-MSI)
    /// delivery mode.
    void defineIdcs();

    /// Called after a CSR write to update masks and bits depending on sourcecfg
    /// if sourcecfg is written. Number of written CSR is passed in csrn.
    void postSourcecfgWrite(unsigned csrn);

    /// Make writable/non-writable the bit corresponding to id in the given
    /// set of CSRs when given flag is true/false.
    void makeWritable(unsigned id, CsrNumber csrn, bool flag)
    {
      if (id == 0 or id > interruptCount_)
        return;
      CsrNumber cn = advance(csrn, id);
      unsigned bitsPerValue = sizeof(CsrValue)*8;
      CsrValue bitMask = CsrValue(1) << (id % bitsPerValue);
      CsrValue mask = csrAt(cn).mask();
      mask = flag? mask | bitMask : mask & ~bitMask;
      csrAt(cn).setMask(mask);
    }

    /// Make writable/non-writable the interrupt-enabled bit corresponding to
    /// id when given flag is true/false.
    void setIeWritable(unsigned id, bool flag)
    { makeWritable(id, CsrNumber::Setie0, flag); }

    /// Make writable/non-writable the interrupt-pending bit corresponding to
    /// id when given flag is true/false.
    void setIpWritable(unsigned id, bool flag)
    { makeWritable(id, CsrNumber::Setip0, flag); }

    /// Advance a csr number by the given amount (add amount to number).
    static CsrNumber advance(CsrNumber csrn, uint32_t amount)
    { return CsrNumber(CsrValue(csrn) + amount); }

    /// Return the value of the interrupt bit (pending or enabled) corresponding
    /// to the given interrupt id and the given CSR which must be the first
    /// CSR in its sequence (i.e. Setip0 or Setie0)
    bool readBit(unsigned id, CsrNumber csrn) const
    {
      unsigned bitsPerItem = sizeof(CsrValue) * 8;
      auto cn = advance(csrn, id/bitsPerItem);
      unsigned bitIx = id % bitsPerItem;
      CsrValue bitMask = CsrValue(1) << bitIx;
      CsrValue value = csrs_.at(unsigned(cn)).read();
      bool ip = value & bitMask;
      return ip;
    }

    /// Set the value of the interrupt bit (pending or enabled) corresponding
    /// to the given interrupt id and the given CSR which must be the first CSR
    /// in its sequence (i.e. Setip0 or Setie0). Caller must check if write is
    /// legal.
    void writeBit(unsigned id, CsrNumber csrn, bool flag)
    {
      unsigned bitsPerItem = sizeof(CsrValue) * 8;
      auto cn = advance(csrn, id/bitsPerItem);
      unsigned bitIx = id % bitsPerItem;
      CsrValue bitMask = CsrValue(1) << bitIx;
      CsrValue value = csrs_.at(unsigned(cn)).read();
      value = flag ? value | bitMask : value & ~bitMask;
      csrs_.at(unsigned(cn)).write(value);
    }

    /// Return the value of the interrupt pending bit corresponding to the
    /// given interrupt id.
    bool readIp(unsigned id) const
    { return readBit(id, CsrNumber::Setip0); }

    /// Set the value of the interrupt pending bit corresponding to the
    /// given interrupt id. Caller must check if write is legal.
    void writeIp(unsigned id, bool flag)
    { if (id) writeBit(id, CsrNumber::Setip0, flag); }

    /// Return the value of the interrupt enabled bit corresponding to the
    /// given interrupt id.
    bool readIe(unsigned id) const
    { return readBit(id, CsrNumber::Setie0); }

    /// Set the value of the interrupt enabled bit corresponding to the
    /// given interrupt id. Caller must check if write is legal.
    void writeIe(unsigned id, bool flag)
    { if (id) writeBit(id, CsrNumber::Setie0, flag); }

  private:

    std::string name_;
    uint64_t addr_ = 0;
    uint64_t size_ = 0;
    unsigned hartCount_ = 0;
    unsigned interruptCount_ = 0;
    unsigned ipriolen_ = MaxIpriolen;
    std::shared_ptr<Domain> parent_ = nullptr;
    bool isMachine_ = true;   // Machine privilege.

    std::vector<DomainCsr> csrs_;
    std::vector<Idc> idcs_;
    std::vector<std::shared_ptr<Domain>> children_;
    std::vector<bool> activeHarts_;  // Hart active in this domain.

    // Callback for direct interrupt delivery.
    std::function<bool(unsigned hartIx, bool machine, bool ip)> deliveryFunc_ = nullptr;

    // Callback for IMSIC interrupt delivery.
    std::function<bool(uint64_t addr, unsigned size, uint64_t data)> imsicFunc_ = nullptr;

  };

}
