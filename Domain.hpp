#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <cassert>


namespace TT_APLIC
{

  /// APLIC domain control ans status register enumeration
  enum class DomainCsrNumber : uint32_t
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


  enum class SourceMode : uint32_t
    {
      Inactive = 0,
      Detached = 1,
      Edge1 = 4,
      Edge0 = 5,
      Level1 = 6,
      Level0 = 7
    };


  /// APLIC Inerrupt Delivery Control (IDC).  One per hart. Used
  /// for direct delivery (non message signaled).
  struct Idc
  {
    uint32_t idelivery_= 0;
    uint32_t iforce_ = 0;
    uint32_t ithreshold_ = 0;
    uint32_t topi_ = 0;
    uint32_t claimi_ = 0;
    uint32_t reserved_[3] = { 0, 0, 0 };
  };


  /// Union to pack/unpack the topi field in Idc.
  union IdcTopi
  {
    IdcTopi(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;   // 1st variant

    struct   // 2nd variant
    {
      unsigned prio_ : 8;
      unsigned res0_ : 8;
      unsigned id_   : 10;
      unsigned res1_ : 6;
    };
  };


  /// Union to pack/unpack the domaincfg CSR.
  union Domaincfg
  {
    Domaincfg(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;    // First variant of union

    struct   // Second variant
    {
      unsigned be_    : 1;  // Big endian
      unsigned res0_  : 1;
      unsigned dm_    : 1;  // Deliver mode
      unsigned res1_  : 4;
      unsigned bit7_  : 1;
      unsigned ie_    : 1;  // Interrupt enable
      unsigned res2_  : 16;
      unsigned top8_  : 8;
    };
  };


  /// Union to pack/unpack the sourcecfg CSRs
  union Sourcecfg
  {
    Sourcecfg(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;  // First variant of union

    struct   // Second variant
    {
      unsigned child_ : 9;    // Child index
      unsigned d_     : 1;    // Delegate
      unsigned res0_  : 22;
    };
  };


  /// Union to pack/unpack the mmsiaddrcfgh CSRs
  union Mmsiaddrcfgh
  {
    Mmsiaddrcfgh(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;

    struct
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
    };
  };


  /// Union to pack/unpack the smsiaddrcfgh CSRs
  union Smsiaddrcfgh
  {
    Smsiaddrcfgh(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;

    struct
    {
      unsigned ppn_  : 12;  // High part of ppn
      unsigned res0_ : 8;
      unsigned lhxs_ : 3;
      unsigned res1_ : 9;
    };
  };


  /// Union to pack/unpack the genmsi CSR
  union Genmsi
  {
    Genmsi(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;

    struct
    {
      unsigned eid_  : 11;  // External interrupt id
      unsigned res0_ : 1;
      unsigned busy_ : 1;
      unsigned res1_ : 5;
      unsigned hart_ : 14;
    };
  };


  /// Union to pack/unpack the target CSRs
  union Target
  {
    Target(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;

    struct
    {
      unsigned prio_ : 8;  // Priority
      unsigned res0_ : 10;
      unsigned hart_ : 14;
    };
  };


  /// Union to pack/unpack the topi register in an IDC structure.
  union Topi
  {
    Topi(uint32_t value)
      : value_(value)
    { }

    uint32_t value_;

    struct
    {
      unsigned prio_ : 8;  // Priority
      unsigned res0_ : 8;
      unsigned hart_ : 10;
      unsigned res1_ : 6;
    };
  };


  /// Aplic domain constrol and status register.
  class DomainCsr
  {
  public:

    /// Default constructor.
    DomainCsr() = default;

    DomainCsr(const std::string& name, DomainCsrNumber csrn,
	      uint32_t reset, uint32_t mask)
      : name_(name), csrn_(csrn), reset_(reset), value_(reset), mask_(mask)
    { }

    /// Return current value of this CSR.
    uint32_t read() const
    { return value_; }

    /// Set value of this CSR to the given value after masking it with
    /// the associated write mask.
    void write(uint32_t value)
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

    /// Return the write maks of thie CSR.
    uint32_t mask() const
    { return mask_; }

  protected:

  private:

    std::string name_;
    DomainCsrNumber csrn_ = DomainCsrNumber{0};
    uint32_t reset_ = 0;
    uint32_t value_ = 0;
    uint32_t mask_ = 0;
  };


  /// Model an advanced platform local interrupt controller domain.
  class Domain
  {
  public:

    /// Aplic domain constants.
    enum { IdcOffset = 0x4000, EndId = 1024 };

    /// Default constructor.
    Domain()
      : active_(32), inverted_(32)
    { }

    /// Constructor. Interrupt count is one plus the largest supported interrupt
    /// id and must be less than ore equal to EndId.
    Domain(uint64_t addr, uint64_t size, unsigned hartCount,
	   unsigned interruptCount, bool hasIdc)
      : addr_(addr), size_(size), hartCount_(hartCount),
	interruptCount_(interruptCount), hasIdc_(hasIdc), active_(32),
	inverted_(32)
    {
      defineCsrs();
      defineIdc();
      assert(interruptCount <= EndId);
    }

    /// Read a memory mapped register associated with this Domain. Return true
    /// on success. Return false leaving value unmodified if addr is not in the
    /// range of this Domain or if size/alignment is not valid.
    bool read(uint64_t addr, unsigned size, uint64_t& value) const;

    /// Write a memory mapped register associated with this Domain. Return true
    /// on success. Return false if addr is not in the range of this Domain or if
    /// size/alignment is not valid.
    bool write(uint64_t addr, unsigned size, uint64_t value);

    /// Initiate an inerrupt with the given interrupt id. Return true on success
    /// return false if id is out of bounds.
    bool initiateInterrupt(unsigned id);

    /// Set the given domain as a child of this domain.
    void setChild(std::shared_ptr<Domain> child)
    { assert(child.get() != this); child_ = child; child->parent_.reset(this); }

    /// Return a pointer to the child domain or nullptr if this domain has
    /// no child.
    std::shared_ptr<Domain> getChild() const
    { return child_; }

    /// Return parent of this domain or nullptr if this is the root domain.
    std::shared_ptr<Domain> getParent() const
    { return parent_; }

    /// Return true if given interrupt id is delegated to a child domain.
    /// Return false if id is out of bounds.
    bool isDelegated(unsigned id) const;

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

  protected:

    /// Set the interrupt pending bit of the given id. Return true if
    /// sucessful. Return false if it is not possible to set the bit (see
    /// secion 4.7 of the riscv-interrupt spec).
    bool trySetIp(unsigned id);

    /// Clear the interrupt pending bit of the given id. Return true if
    /// sucessful. Return false if it is not possible to set the bit (see
    /// secion 4.7 of the riscv-interrupt spec).
    bool tryClearIp(unsigned id);

    /// Set the interrupt pending bit corresonding to the given interrupt id to
    /// flag. Return true on sucess and false if id is out of bounds. This has
    /// no effect if the interrupt id is not active in this domain. The top id
    /// for the target host will be updated as a side effect.
    bool setInterruptPending(unsigned id, bool flag);

    /// Return true if this domain targets machine privilege: If root domain or
    /// domain has a child, then its privilege is machine; otherwise, it is
    /// supervisor.
    bool isMachinePrivilege() const
    { return isRoot() or not isLeaf(); }

    /// Return true if this is domain is a leaf.
    bool isLeaf() const
    { return not getChild(); }

    /// Return true if this is domain is a root domain.
    bool isRoot() const
    { return not getParent(); }

    /// Return CSR having the given number n.
    DomainCsr& csrAt(DomainCsrNumber n)
    { return csrs_.at(unsigned(n)); }

    /// Define the control and status memory mapped registers associated with
    /// this domain.
    void defineCsrs();

    /// Define the interrupt deliver control structures (one per hart)
    /// associated with this domain. IDC is used only in direct (non-MSI)
    /// delivery mode.
    void defineIdc();

    /// Advance a csr number by the given amount (add amount to number).
    static DomainCsrNumber advance(DomainCsrNumber csrn, uint32_t amount)
    { return DomainCsrNumber(uint32_t(csrn) + amount); }

    /// Advance a csr number by the given amount (add amount to number).
    static DomainCsrNumber advance(DomainCsrNumber csrn, int32_t amount)
    { return DomainCsrNumber(uint32_t(csrn) + amount); }

  private:

    uint64_t addr_ = 0;
    uint64_t size_ = 0;
    unsigned hartCount_ = 0;
    unsigned interruptCount_ = 0;
    bool hasIdc_ = false;

    std::vector<DomainCsr> csrs_;
    std::vector<Idc> idcs_;
    std::shared_ptr<Domain> child_;
    std::shared_ptr<Domain> parent_;
    std::vector<uint32_t> active_;  // 32 words, parallel to setip0-setip31
    std::vector<uint32_t> inverted_;  // 32 words, parallel to setip0-setip31
  };

}
