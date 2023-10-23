#include <cassert>
#include <stdexcept>
#include "Aplic.hpp"


using namespace TT_APLIC;


Aplic::Aplic(uint64_t addr, uint64_t stride, unsigned hartCount,
	     unsigned domainCount, unsigned interruptCount)
  : addr_(addr), stride_(stride), size_(stride*hartCount), hartCount_(hartCount),
    domainCount_(domainCount), interruptCount_(interruptCount)
{
  if ((addr & 0xfff) != 0)
    throw std::runtime_error("Invalid aplic address -- must be a multiple of 4096");

  if ((stride & 0xfff) != 0)
    throw std::runtime_error("Invalid aplic stride -- must be a multiple of 4096");

  unsigned domainSize = Domain::IdcOffset + hartCount * sizeof(Idc);
  if (stride < domainSize)
    throw std::runtime_error("Invalid aplic stride -- too small for given hart count");

  regionDomains_.resize(domainCount);
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
  if (id == 0 or id >= interruptCount_ or not root_)
    return false;
  return root_->setSourceState(id, state);
}


std::shared_ptr<Domain>
Aplic::createDomain(std::shared_ptr<Domain> parent, uint64_t addr, bool isMachine)
{
  if ((addr % stride_) != 0)
    return nullptr;

  unsigned regionIx = 0;
  if (not findRegionByAddr(addr, regionIx))
    return nullptr;    // Addr is out of bounds.

  if (regionDomains_.at(regionIx))
    return nullptr;    // Regoin of addr already occupied.

  if (not root_ and parent)
    return nullptr;   // First created domain must be root.

  if (not root_ and not isMachine)
    return nullptr;   // Root domain must be at machine privilege.

  if (not isMachine and not parent)
    return nullptr;   // Supervisor domain must not be root.

  if (not isMachine and not parent->isMachinePrivilege())
    return nullptr;   // Supervisor parent must be machine.

  std::shared_ptr<Domain> domain = std::make_shared<Domain>(parent, addr, stride_,
							    hartCount_,
							    interruptCount_,
							    isMachine);
  regionDomains_.at(regionIx) = domain;
  if (not root_)
    root_ = domain;

  if (parent)
    parent->addChild(domain);

  return domain;
}
