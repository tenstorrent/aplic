#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Domain.hpp"


namespace TT_APLIC
{

  /// Model an advanced platform local interrupt controlled
  class Aplic
  {
  public:

    /// Constructor. Interrupt count is one plus the largest supported interrupt
    /// id and must be less than ore equal to 1024. Stride is the offset in bytes
    /// between the starting address of two consecutive domains. The address
    /// space region occupied by this Aplic has a size of n = domainCount *
    /// stride and occupies the addresses addr to addr + n - 1.
    Aplic(uint64_t addr, uint64_t stride, unsigned hartCount,
	  unsigned domainCount, unsigned interruptCount, bool hasIdc);

    /// Read a memory mapped register associated with this Domain. Return true
    /// on success. Return false leaving value unmodified if addr is not in the
    /// range of this Domain or if size/alignment is not valid.
    bool read(uint64_t addr, unsigned size, uint64_t& value) const;

    /// Write a memory mapped register associated with this Domain. Return true
    /// on success. Return false if addr is not in the range of this Domain or if
    /// size/alignment is not valid.
    bool write(uint64_t addr, unsigned size, uint64_t value);

    /// Set the state of the source of the given id to the given value.  Return
    /// true on success. Return false if id is out of bounds.  If the the state
    /// is equal to the state at which the source is active then the
    /// corresponding interrupt becomes pending.
    bool setSourceState(unsigned id, bool state);

  protected:

    /// Return a pointer to the domain covering the given address. Return
    /// nullptr if the address is not valid (must be word aligned) or is out of
    /// bounds.
    Domain* findDomainByAddr(uint64_t addr)
    {
      if ((addr & 3) != 0 or addr < addr_ or addr - addr_ >= size_)
	return nullptr;
      uint64_t domainIx = (addr - addr_) / stride_;
      return &domains_.at(domainIx);
    }

    /// Same as above but constant.
    const Domain* findDomainByAddr(uint64_t addr) const
    {
      if ((addr & 3) != 0 or addr < addr_ or addr - addr_ >= size_)
	return nullptr;
      uint64_t domainIx = (addr - addr_) / stride_;
      return &domains_.at(domainIx);
    }


  private:

    uint64_t addr_ = 0;
    uint64_t stride_ = 0;
    uint64_t size_ = 0;
    unsigned hartCount_ = 0;
    unsigned domainCount_ = 0;
    unsigned interruptCount_ = 0;
    bool hasIdc_ = false;
    std::vector<Domain> domains_;
  };
}
