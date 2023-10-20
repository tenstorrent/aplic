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
      Mmsiaddrcfg,
      Mmsiaddrcfgh,
      Smsiaddrcfg,
      Smsiaddrcfgh,
      Setip0,
      Setip31 = Setip0 + 31,
      Setipnum,
      Inclrip0,
      Inclrip31 = Inclrip0 + 31,
      Clripnum,
      Setie0,
      Setie31 = Setie0 + 31,
      Setienum,
      Clrie0,
      Clrie31 = Clrie0 + 31,
      Clrienum,
      Setipnumle,
      Setipnumbe,
      Genmsi,
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
    CsrValue idelivery_= 0;
    CsrValue iforce_ = 0;
    CsrValue ithreshold_ = 0;
    CsrValue topi_ = 0;
    CsrValue claimi_ = 0;
    CsrValue reserved_[3] = { 0, 0, 0 };
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
    Domaincfg(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits.
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
    Sourcecfg(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits when delegated (D == 1).
    constexpr static CsrValue delegatedMask()
    { return 0b111'1111'1111; }

    /// Mask of writeable bits when not delegated (D == 0).
    constexpr static CsrValue nonDelegatedMask()
    { return 0b100'0000'0011; }

    CsrValue value_;  // First variant of union

    struct   // Second variant
    {
      unsigned child_ : 9;    // Child index (relative to parent)
      unsigned d_     : 1;    // Delegate
      unsigned res0_  : 22;
    } bits_;

    struct   // Third variant
    {
      unsigned sm_     : 2;  // Source mode
      unsigned unused_ : 30;
    } bits2_;
  };


  /// Union to pack/unpack the Mmsiaddrcfgh CSR
  union Mmsiaddrcfgh
  {
    Mmsiaddrcfgh(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits.
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
    Smsiaddrcfgh(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits.
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
    Genmsi(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits.
    constexpr static CsrValue mask()
    { return 0b1111'1111'1111'1100'0001'0111'1111'1111; }

    CsrValue value_;   // First union variant

    struct             // Second variant
    {
      unsigned eid_  : 11;  // External interrupt id
      unsigned res0_ : 1;
      unsigned busy_ : 1;
      unsigned res1_ : 5;
      unsigned hart_ : 14;
    } bits_;
  };


  /// Union to pack/unpack the Target CSRs
  union Target
  {
    Target(CsrValue value)
      : value_(value)
    { }

    /// Mask of writeable bits.
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
      unsigned eeid_  : 11;
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

    /// Aplic domain constants.
    enum { IdcOffset = 0x4000, EndId = 1024, EndHart = 16384, Align = 0xfff,
	   MaxIpriolen = 8 };

    /// Default constructor.
    Domain()
    { }

    /// Constructor. Interrupt count is one plus the largest supported interrupt
    /// id and must be less than ore equal to EndId. Size is the number of bytes
    /// occupied by this domain in the memory address space.
    Domain(uint64_t addr, uint64_t size, unsigned hartCount,
	   unsigned interruptCount, bool hasIdc)
      : addr_(addr), size_(size), hartCount_(hartCount),
	interruptCount_(interruptCount), hasIdc_(hasIdc)
    {
      defineCsrs();
      if (hasIdc)
	defineIdcs();
      assert(interruptCount <= EndId);
      assert(hartCount <= EndHart);
      assert((addr % Align) == 0);  // Address must be a aligned.
      assert((size % Align) == 0);  // Size must be a multiple of alignment.
      if (hasIdc)
	assert(size >= IdcOffset + hartCount * sizeof(Idc));
      else
	assert(size >= IdcOffset);
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

    /// Set the given domain as a child of this domain.
    void addChild(std::shared_ptr<Domain> child)
    {
      assert(child.get() != this);
      assert(not child->parent_);
      children_.push_back(child); child->parent_.reset(this);
    }

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
    bool setSourceState(unsigned id, bool state);

    /// Return the source state of the interrupt source with the given id.
    SourceMode sourceMode(unsigned id) const;

    /// Return true if interrupt with given id is active (enabled) in this
    /// domain.
    bool isActive(unsigned id) const
    { return id != 0 and id < interruptCount_ and not isDelegated(id) and
	sourceMode(id) != SourceMode::Inactive; }

    /// Return true if interrupt with given id is inverted in this domain
    /// (active low).
    bool isInverted(unsigned id) const
    {
      using SM = SourceMode;
      return id != 0 and id < interruptCount_ and not isDelegated(id) and
	(SM(id) == SM::Edge0 or SM(id) == SM::Level0);
    }

    /// Return true if interrupt with given id is level sensitive this domain.
    bool isLevelSensitive(unsigned id) const
    {
      using SM = SourceMode;
      return id != 0 and id < interruptCount_ and not isDelegated(id) and
	(SM(id) == SM::Level0 or SM(id) == SM::Level1);
    }

    /// Define a callback function for the domain to deliver an interrupt to a
    /// hart. When an interrupt becomes active (ready for delivery), the domain
    /// will call this function which will presumably set the M/S external
    /// interrupt pending bit in the MIP CSR of that hart.
    void setDeliverMethod(std::function<bool(unsigned hartIx, bool machine)> func)
    { deliveryFunc_ = func; }

    /// Return the IMSIC address for the given hart. This is computed from the
    /// MMSIADDRCFG CSRs for machine privilegeand from the SMSIADDRCFG CSRS for
    /// supervisor privilege domains. See section 4.9.1 of the riscv spec.
    uint64_t imsicAddress(unsigned hartIx);

    /// Return true if this domain is in big-endian configuration.
    bool bigEndian() const
    { return domaincfg().bits_.be_; }

    /// Return true if interrupts are enabled for this domain.
    bool interruptEnabled() const
    { return domaincfg().bits_.ie_; }

    /// Return true if deilery mode is direct for this mode. Return false if
    /// delivery mode is through MSI.
    bool directDelivery() const
    { return domaincfg().bits_.dm_; }

  protected:

    /// Return the domaincfg CSR value.
    Domaincfg domaincfg() const
    { return csrs_.at(unsigned(CsrNumber::Domaincfg)).read(); }

    /// Return the pointer to the root domain.
    std::shared_ptr<Domain> rootDomain()
    {
      if (isRoot())
	return std::shared_ptr<Domain>(this);
      return getParent()->rootDomain();
    }

    /// Helper to read method. Read from the interrupt delivery control section.
    bool readIdc(uint64_t addr, unsigned size, CsrValue& value);

    /// Helper to write method. Write to the interrupt delivery control section.
    bool writeIdc(uint64_t addr, unsigned size, CsrValue value);

    /// Set the interrupt pending bit of the given id. Return true if
    /// sucessful. Return false if it is not possible to set the bit (see
    /// secion 4.7 of the riscv-interrupt spec).
    bool trySetIp(unsigned id);

    /// Clear the interrupt pending bit of the given id. Return true if
    /// sucessful. Return false if it is not possible to set the bit (see
    /// secion 4.7 of the riscv-interrupt spec).
    bool tryClearIp(unsigned id);

    /// Set the interrupt pending bit corresponding to the given interrupt id to
    /// flag. Return true on success and false if id is out of bounds. This has
    /// no effect if the interrupt id is not active in this domain. The top id
    /// for the target host will be updated as a side effect.
    bool setInterruptPending(unsigned id, bool flag);

    /// Return true if this domain targets machine privilege.
    bool isMachinePrivilege() const
    { return isMachine_; }

    /// Set the privilege level of this domain to machine if flag is true;
    /// otherwise set it to supervisor privilege.
    void setMachinePrivilege(bool flag)
    { isMachine_ = flag; }

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

    /// Advance a csr number by the given amount (add amount to number).
    static CsrNumber advance(CsrNumber csrn, uint32_t amount)
    { return CsrNumber(CsrValue(csrn) + amount); }

    /// Advance a csr number by the given amount (add amount to number).
    static CsrNumber advance(CsrNumber csrn, int32_t amount)
    { return CsrNumber(CsrValue(csrn) + amount); }

  private:

    uint64_t addr_ = 0;
    uint64_t size_ = 0;
    unsigned hartCount_ = 0;
    unsigned interruptCount_ = 0;
    unsigned ipriolen_ = MaxIpriolen;
    bool hasIdc_ = false;
    bool isMachine_ = true;   // Machine privilege.

    std::vector<DomainCsr> csrs_;
    std::vector<Idc> idcs_;
    std::vector<std::shared_ptr<Domain>> children_;
    std::shared_ptr<Domain> parent_;

    /// Callback to deliver an external interrupt to a hart.
    std::function<bool(unsigned hartIx, bool machine)> deliveryFunc_ = nullptr;

  };

}
