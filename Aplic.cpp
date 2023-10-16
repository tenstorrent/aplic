#include <cassert>
#include <stdexcept>
#include "Aplic.hpp"


using namespace TT_APLIC;


Aplic::Aplic(uint64_t addr, uint64_t stride, unsigned hartCount,
	     unsigned domainCount, unsigned interruptCount, bool hasIdc)
  : addr_(addr), stride_(stride), size_(stride*hartCount), hartCount_(hartCount),
    domainCount_(domainCount), interruptCount_(interruptCount), hasIdc_(hasIdc)
{
  if ((addr & 0xfff) != 0)
    throw std::runtime_error("Invalid aplic address -- must be a multiple of 4096");

  if ((stride & 0xfff) != 0)
    throw std::runtime_error("Invalid aplic stride -- must be a multiple of 4096");

  domains_.resize(domainCount);
}


bool
Aplic::read(uint64_t addr, unsigned size, uint64_t& value) const
{
  if ((addr & 3) != 0 or size != 4)
    return false;  // Address must be word aligned. Size must be word.

  const Domain* domain = findDomainByAddr(addr);
  if (not domain)
    return false;

  return domain->read(addr, size, value);
}


bool
Aplic::write(uint64_t addr, unsigned size, uint64_t value)
{
  if ((addr & 3) != 0 or size != 4)
    return false;  // Address must be word aligned. Size must be word.

  Domain* domain = findDomainByAddr(addr);
  if (not domain)
    return false;

  return domain->write(addr, size, value);
}


bool
Aplic::setSourceState(unsigned id, bool state)
{
  if (id == 0 or id >= interruptCount_ or domains_.empty())
    return false;

  auto& domain = domains_.at(0);
  return domain.setSourceState(id, state);
}
