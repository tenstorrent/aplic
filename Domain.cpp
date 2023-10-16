#include "Domain.hpp"

using namespace TT_APLIC;

bool
Domain::read(uint64_t addr, unsigned size, uint64_t& value) const
{
  value = 0;

  if ((addr & 3) != 0 or size != 4)
    return false;

  if (addr < addr_ or addr - addr_ >= size)
    return false;

  uint64_t wordIx = (addr - addr_) / 4;
  if (wordIx < csrs_.size())
    {
      value = csrs_.at(wordIx).read();
      return true;
    }

  if (not hasIdc_)
    return false;

  uint64_t idcIndex = (addr - (addr_ + IdcOffset)) / sizeof(Idc);
  if (idcIndex < idcs_.size())
    {
      const Idc& idc = idcs_.at(idcIndex);
      uint64_t idcWordCount = sizeof(idc) / 4;
      uint64_t idcWord = wordIx % idcWordCount;
      switch (idcWord)
	{
	case 0  :
	  value = idc.idelivery_;
	  break;
	case 1  :
	  value = idc.iforce_;
	  break;
	case 2  :
	  value = idc.ithreshold_;
	  break;
	case 3  :
	  value = idc.topi_;
	  if (value >= idc.ithreshold_)
	    value = 0;
	  break;
	case 4  :
	  value = idc.claimi_;
	  break;
	default :
	  value = 0;
	  break;
	}
      return true;
    }

  return false;
}


bool
Domain::write(uint64_t addr, unsigned size, uint64_t value)
{
  using CN = DomainCsrNumber;

  if ((addr & 3) != 0 or size != 4)
    return false;

  if (addr < addr_ or addr - addr_ >= size)
    return false;

  uint64_t wordIx = (addr - addr_) / 4;
  if (wordIx < csrs_.size())
    {
      if (wordIx >= unsigned(CN::Sourcecfg1) and
	  wordIx <= unsigned(CN::Sourcecfg1023))
	{
	  if (isLeaf())
	    value = 0;
	}

      if (wordIx >= unsigned(CN::Setip0) and
	  wordIx <= unsigned(CN::Setip31))
	value = csrs_.at(wordIx).read() | (value & active_.at(wordIx));
      else if (wordIx == unsigned(CN::Setipnum))
	{
	  if (not isActive(value) or isLevelSensitive(value))
	    return true;
	  unsigned ix = unsigned(CN::Setip0) + value / 32;
	  unsigned bit = value % 32;
	  csrs_.at(ix).write(csrs_.at(ix).read() | (uint32_t(1) << bit));
	  return true;
	}
      else if (wordIx == unsigned(CN::Clripnum))
	{
	  if (not isActive(value) or isLevelSensitive(value))
	    return true;
	  unsigned ix = unsigned(CN::Setip0) + value / 32;
	  unsigned bit = value % 32;
	  csrs_.at(ix).write(csrs_.at(ix).read() & ~(uint32_t(1) << bit));
	  return true;
	}

      csrs_.at(wordIx).write(value);

      // Writing sourcecfg may change a source status. Cache status.
      if (wordIx >= unsigned(CN::Sourcecfg1) and
	  wordIx <= unsigned(CN::Sourcecfg1023))
	{
	  unsigned id = wordIx - unsigned(CN::Sourcecfg1) + 1;
	  unsigned ix = id / 32;
	  bool flag = isActive(id);
	  uint32_t mask = uint32_t(1) << ix % 32;
	  active_.at(ix) = flag ? active_.at(ix) | mask : active_.at(ix) & ~mask;
	  inverted_.at(ix) = isInverted(id);
	}

      return true;
    }

  if (not hasIdc_)
    return false;

  uint64_t idcIndex = (addr - (addr_ + IdcOffset)) / sizeof(Idc);
  if (idcIndex < idcs_.size())
    {
      Idc& idc = idcs_.at(idcIndex);
      uint64_t idcWordCount = sizeof(idc) / 4;
      uint64_t idcWord = wordIx % idcWordCount;
      switch (idcWord)
	{
	case 0  : idc.idelivery_  = value; break;
	case 1  : idc.iforce_     = value; break;
	case 2  : idc.ithreshold_ = value; break;
	case 3  : idc.topi_       = value; break;
	case 4  : idc.claimi_     = value; break;
	default :                          break;
	}
      return true;
    }

  return false;
}


void
Domain::defineCsrs()
{
  using CN = DomainCsrNumber;

  csrs_.resize(size_t(CN::Target1023) + 1);

  uint32_t allOnes = ~uint32_t(0);

  csrAt(CN::Domaincfg) = DomainCsr("domaincfg", CN::Domaincfg, 0, allOnes);

  std::string base = "sourcecfg";
  for (unsigned ix = 1; ix <= 1023; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Sourcecfg1) + ix - 1};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }

  csrAt(CN::Mmsiaddrcfg) = DomainCsr("mmsiaddrcfg", CN::Mmsiaddrcfg, 0, allOnes);
  csrAt(CN::Mmsiaddrcfgh) = DomainCsr("mmsiaddrcfgh", CN::Mmsiaddrcfgh, 0, allOnes);
  csrAt(CN::Smsiaddrcfg) = DomainCsr("smsiaddrcfg", CN::Smsiaddrcfg, 0, allOnes);
  csrAt(CN::Smsiaddrcfgh) = DomainCsr("smsiaddrcfgh", CN::Smsiaddrcfgh, 0, allOnes);

  base = "setip";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Setip0) + ix};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }
  csrAt(CN::Setipnum) = DomainCsr("setipnum", CN::Setipnum, 0, allOnes);

  base = "in_clrip";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Inclrip0) + ix};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }
  csrAt(CN::Clripnum) = DomainCsr("clripnum", CN::Clripnum, 0, allOnes);

  base = "setie";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Setie0) + ix};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }
  csrAt(CN::Setienum) = DomainCsr("setienum", CN::Setienum, 0, allOnes);

  base = "clrie";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Clrie0) + ix};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }
  csrAt(CN::Clrienum) = DomainCsr("clrienum", CN::Clrienum, 0, allOnes);

  csrAt(CN::Setipnumle) = DomainCsr("setipnum_le", CN::Setipnumle, 0, allOnes);
  csrAt(CN::Setipnumbe) = DomainCsr("setipnum_be", CN::Setipnumbe, 0, allOnes);
  csrAt(CN::Genmsi) = DomainCsr("genmsi", CN::Genmsi, 0, allOnes);
  
  base = "target";
  for (unsigned ix = 1; ix <= 1023; ++ix)
    {
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Target1) + ix - 1};
      csrAt(cn) = DomainCsr(name, cn, 0, allOnes);
    }
}


bool
Domain::setSourceState(unsigned id, bool state)
{
  if (id >= interruptCount_ or id == 0)
    return false;

  if (isDelegated(id))
    return child_->setSourceState(id, state);

  // Determine interrupt target and priority.
  using CN = DomainCsrNumber;
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};
  unsigned prio =  target.prio_;
  unsigned hart = target.hart_;

  SourceMode mode = sourceMode(id);
  bool activate = (mode == SourceMode::Edge1 or mode == SourceMode::Level1) == state;

  // Mark/unmark interrupt pending.
  CN nipc = advance(CN::Setip0, id);  // Number of inerrupt pending CSR.
  uint32_t ipVal = csrAt(nipc).read();
  uint32_t mask = uint32_t(1) << (id % 32);
  ipVal = activate? (ipVal | mask) : (ipVal & ~mask);
  csrAt(nipc).write(ipVal);

  if (not hasIdc_)
    {
      assert(0 && "Implement: deliver MSI");
      return true;
    }

  // Update top priority interrupt. Lower priority number wins.
  // For tie, lower source id wins
  auto& idc = idcs_.at(hart);
  IdcTopi topi{idc.topi_};
  unsigned topPrio = topi.prio_;
  if (activate)
    {
      if (prio <= topPrio or (prio == topPrio and id < topi.id_))
	{
	  topi.prio_ = prio;
	  topi.id_ = id;
	  idc.topi_ = topi.value_;
	}
    }
  else if (id == topi.id_)
    {
      // Interrupt that used to determine our top id went away. Re-compute
      // top id.
      bool found = false;
      for (unsigned i = 1; i < interruptCount_; ++i)
	{
	  CN ntc = advance(CN::Target1, i - 1);
	  uint32_t targetVal = csrAt(ntc).read();
	  Target target{targetVal};
	  if (target.hart_ == hart)
	    if (not found or target.prio_ < topi.prio_)
	      {
		found = true;
		topi.prio_ = target.prio_;
		topi.id_ = i;
	      }
	}
    }

  return true;
}


bool
Domain::isDelegated(unsigned id) const
{
  if (id >= interruptCount_ or id == 0)
    return false;

  using CN = DomainCsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  // Check if source is delegated.
  auto configVal = csrs_.at(unsigned(cn)).read();
  Sourcecfg sc{configVal};
  if (sc.d_)
    return true;
  return false;
}


SourceMode
Domain::sourceMode(unsigned id) const
{
  if (id >= interruptCount_ or id == 0)
    return SourceMode::Inactive;

  using CN = DomainCsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  // Check if source is delegated.
  auto configVal = csrs_.at(unsigned(cn)).read();
  Sourcecfg sc{configVal};
  if (sc.d_)
    return SourceMode::Inactive;

  /// Least significant 3 bits encode the source mode.
  return SourceMode{configVal & 7};
}
