#include <cassert>
#include <stdexcept>
#include "Aplic.hpp"


using namespace TT_APLIC;


Aplic::Aplic(unsigned hartCount, unsigned interruptCount)
  : hartCount_(hartCount), interruptCount_(interruptCount)
{
  interruptStates_.resize(interruptCount+1);
}


bool
Aplic::read(uint64_t addr, unsigned size, uint64_t& value)
{
  if ((addr & 3) != 0 or size != 4)
    return false;  // Address must be word aligned. Size must be word.

  auto domain = findDomainByAddr(addr);
  if (not domain)
    return false;

  return domain->read(addr, size, value);
}


bool
Aplic::write(uint64_t addr, unsigned size, uint64_t value)
{
  if ((addr & 3) != 0 or size != 4)
    return false;  // Address must be word aligned. Size must be word.

  auto domain = findDomainByAddr(addr);
  if (not domain)
    return false;

  return domain->write(addr, size, value);
}


bool
Aplic::setSourceState(unsigned id, bool state)
{
  if (id == 0 or id > interruptCount_ or not root_)
    return false;

  bool prev = interruptStates_.at(id);
  if (not root_->setSourceState(id, prev, state))
    return false;
  interruptStates_.at(id) = state;
  return true;
}


std::shared_ptr<Domain>
Aplic::createDomain(const std::string& name, std::shared_ptr<Domain> parent, uint64_t addr, uint64_t size, bool isMachine)
{
  if (addr % 4096 != 0)
    return nullptr;

  if (size < 16*1024)
    return nullptr;

  if (size % 4096 != 0)
    return nullptr;

  // TODO: check for overlap with other domains

  if (not root_ and parent)
    return nullptr;   // First created domain must be root.

  if (not root_ and not isMachine)
    return nullptr;   // Root domain must be at machine privilege.

  if (not isMachine and not parent)
    return nullptr;   // Supervisor domain must not be root.

  if (not isMachine and not parent->isMachinePrivilege())
    return nullptr;   // Supervisor parent must be machine.

  if (root_ and not parent)
    return nullptr;   // Cannot have more than one root.

  auto domain = std::make_shared<Domain>(name, parent, addr, size, hartCount_,
                                         interruptCount_, isMachine);

  domain->setImsicMethod(imsicFunc_);
  domain->setDeliveryMethod(deliveryFunc_);

  domains_.push_back(domain);

  if (not root_)
    root_ = domain;

  if (parent)
    parent->addChild(domain);

  domain->setDeliveryMethod(deliveryFunc_);
  return domain;
}
