#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "Domain.hpp"


namespace TT_APLIC
{

  struct Interrupt
  {
    Domain *domain;
    unsigned id;
  };

  /// Model an advanced platform local interrupt controller
  class Aplic
  {
  public:

    /// Constructor. interruptCount is the largest supported interrupt id and
    /// must be less than or equal to 1023.
    Aplic(unsigned hartCount, unsigned interruptCount, bool autoDeliver);

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
                                         uint64_t addr, uint64_t size, bool isMachine);

    bool autoDeliveryEnabled() { return autoDeliver_; }
    void enableAutoDelivery()
    {
      for (auto i : undeliveredInterrupts_)
        i.domain->deliverInterrupt(i.id);
      autoDeliver_ = true;
    }
    void disableAutoDelivery() { autoDeliver_ = false; }

    void enqueueInterrupt(Domain *domain, unsigned id);
    bool deliverInterrupt(unsigned id);

    /// Define a callback function for this Aplic to directly deliver/un-deliver an
    /// interrupt to a hart. When an interrupt becomes active (ready for delivery) or
    /// inactive, the Aplic will call this function which will should set/clear the M/S
    /// external interrupt pending bit in the MIP CSR of that hart.
    void setDeliveryMethod(std::function<bool(unsigned hartIx, bool machine, bool ip)> func)
    {
      deliveryFunc_ = func;
      for (auto domain : domains_)
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
      for (auto domain : domains_)
        if (domain)
          domain->setImsicMethod(func);
    }

    bool contains_addr(uint64_t addr)
    {
      for (auto domain : domains_)
        if (domain->contains_addr(addr))
          return true;
      return false;
    }

  protected:

    /// Return a pointer to the domain covering the given address. Return
    /// nullptr if the address is not valid (must be word aligned) or is out of
    /// bounds.
    std::shared_ptr<Domain> findDomainByAddr(uint64_t addr)
    {
      for (auto domain : domains_)
        if (domain->contains_addr(addr))
          return domain;
      return nullptr;
    }

  private:

    unsigned hartCount_ = 0;
    unsigned interruptCount_ = 0;
    bool autoDeliver_ = false;
    std::shared_ptr<Domain> root_ = nullptr;
    std::vector<std::shared_ptr<Domain>> domains_;

    // Current state of interrupt sources
    std::vector<bool> interruptStates_;

    // Interrupts ready to be delivered
    std::vector<Interrupt> undeliveredInterrupts_;

    // Callback for direct interrupt delivery.
    std::function<bool(unsigned hartIx, bool machine, bool ip)> deliveryFunc_ = nullptr;

    // Callback for IMSIC interrupt delivery.
    std::function<bool(uint64_t addr, unsigned size, uint64_t data)> imsicFunc_ = nullptr;
  };
}
