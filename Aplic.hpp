#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "Domain.hpp"


namespace TT_APLIC
{

  /// Model an advanced platform local interrupt controller
  class Aplic
  {
  public:

    /// Constructor. interruptCount is the largest supported interrupt id and
    /// must be less than or equal to 1023. Stride is the offset in bytes
    /// between the starting address of two consecutive domains. The address
    /// space region occupied by this Aplic has a size of n = domainCount *
    /// stride and occupies the addresses addr to addr + n - 1.
    Aplic(uint64_t addr, uint64_t stride, unsigned hartCount,
	  unsigned domainCount, unsigned interruptCount);

    /// Read a memory mapped register associated with this Aplic. Return true
    /// on success. Return false leaving value unmodified if addr is not in the
    /// range of this Aplic or if size/alignment is not valid.
    bool read(uint64_t addr, unsigned size, uint64_t& value);

    /// Write a memory mapped register associated with this Aplic. Return true
    /// on success. Return false if addr is not in the range of this Aplic or if
    /// size/alignment is not valid.
    bool write(uint64_t addr, unsigned size, uint64_t value);

    /// Set the state of the source of the given id to the given value. Return true on
    /// success. Return false if id is out of bounds. If the state is equal to the state
    /// at which the source is active then the corresponding interrupt becomes pending.
    bool setSourceState(unsigned id, bool state);

    /// Create a domain and make it a child of the given parent. Create a root domain if
    /// parent is empty. Root domain must be created before all other domain and must have
    /// machine privilege. A parent domain must be created before its child. Return
    /// pointer to created domain or nullptr if we fail to create a domain.
    std::shared_ptr<Domain> createDomain(const std::string& name, std::shared_ptr<Domain> parent,
					 uint64_t addr, bool isMachine);

    /// Define a callback function for this Aplic to directly deliver/un-deliver an
    /// interrupt to a hart. When an interrupt becomes active (ready for delivery) or
    /// inactive, the Aplic will call this function which will should set/clear the M/S
    /// external interrupt pending bit in the MIP CSR of that hart.
    void setDeliveryMethod(std::function<bool(unsigned hartIx, bool machine, bool ip)> func)
    {
      deliveryFunc_ = func;
      for (auto domain : regionDomains_)
	if (domain)
	  domain->setDeliveryMethod(func);
    }

    /// Define a callback function for this Aplic to write to the IMSIC of a hart. When an
    /// interrupt becomes active (ready for delivery), the Aplic will call this function
    /// which should write to an IMIC address to set the M/S external interrupt pending
    /// bit in the interrupt file of that IMSIC.
    void setImsicMethod(std::function<bool(uint64_t addr, unsigned size, uint64_t data)> func)
    {
      imsicFunc_ = func;
      for (auto domain : regionDomains_)
	if (domain)
	  domain->setImsicMethod(func);
    }

    bool contains_addr(uint64_t addr)
    {
      return addr >= addr_ and addr < addr_ + size_;
    }

  protected:

    /// Return a pointer to the domain covering the given address. Return
    /// nullptr if the address is not valid (must be word aligned) or is out of
    /// bounds.
    std::shared_ptr<Domain> findDomainByAddr(uint64_t addr)
    {
      unsigned regionIx = 0;
      if (not findRegionByAddr(addr, regionIx))
	return nullptr;
      return regionDomains_.at(regionIx);
    }

    /// Find the index of the memory sub-region associated with the given
    /// address. The Aplic memory region is divided into consecutive sub-regions
    /// each of size domainSize. Return true on success.  Return false if
    /// address is outside the memory region of this Aplic.
    bool findRegionByAddr(uint64_t addr, unsigned& index) const
    {
      if (addr < addr_ or addr >= addr_ + stride_*domainCount_)
	return false;
      index = (addr - addr_) / stride_;
      return true;
    }

  private:

    uint64_t addr_ = 0;
    uint64_t stride_ = 0;    // Domain size.
    uint64_t size_ = 0;
    unsigned hartCount_ = 0;
    unsigned domainCount_ = 0;
    unsigned interruptCount_ = 0;
    std::shared_ptr<Domain> root_ = nullptr;
    // Vector of domains indexed by memory region.
    std::vector<std::shared_ptr<Domain>> regionDomains_;

    // Current state of interrupt sources
    std::vector<bool> interruptStates_;

    // Callback for direct interrupt delivery.
    std::function<bool(unsigned hartIx, bool machine, bool ip)> deliveryFunc_ = nullptr;

    // Callback for IMSIC interrupt delivery.
    std::function<bool(uint64_t addr, unsigned size, uint64_t data)> imsicFunc_ = nullptr;
  };
}
