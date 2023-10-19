#include "Domain.hpp"

using namespace TT_APLIC;

bool
Domain::read(uint64_t addr, unsigned size, uint64_t& value)
{
  value = 0;

  unsigned reqSize = sizeof(CsrValue);  // Required size.

  // Check size and alignment.
  if (size != reqSize or (addr & (reqSize - 1)) != 0)
    return false;

  if (addr < addr_ or addr - addr_ >= size)
    return false;

  uint64_t itemIx = (addr - addr_) / reqSize;
  if (itemIx < csrs_.size())
    {
      value = csrs_.at(itemIx).read();
      return true;
    }

  return readIdc(addr, size, value);
}


bool
Domain::write(uint64_t addr, unsigned size, uint64_t value)
{
  using CN = CsrNumber;

  unsigned reqSize = sizeof(CsrValue);  // Required size.

  // Check size and alignment.
  if (size != reqSize or (addr & (reqSize - 1)) != 0)
    return false;

  if (addr < addr_ or addr - addr_ >= size)
    return false;

  unsigned bitsPerItem = reqSize*8;

  uint64_t itemIx = (addr - addr_) / reqSize;
  if (itemIx < csrs_.size())
    {
      if (itemIx >= unsigned(CN::Sourcecfg1) and
	  itemIx <= unsigned(CN::Sourcecfg1023))
	{
	  if (isLeaf())
	    value = 0;
	}

      if (itemIx >= unsigned(CN::Setip0) and itemIx <= unsigned(CN::Setip31))
	{
	  unsigned id0 = (itemIx - unsigned(CN::Setip0)) * bitsPerItem;
	  for (unsigned bitIx = 0; bitIx < bitsPerItem; ++bitIx)
	    if ((value >> bitIx) & 1)
	      trySetIp(id0 + bitIx);
	  return true;
	}
      else if (itemIx == unsigned(CN::Setipnum))
	{
	  trySetIp(value);  // Value is the interrupt id.
	  return true;  // Setipnum CSR is not updated (read zero).
	}
      if (itemIx >= unsigned(CN::Inclrip0) and itemIx <= unsigned(CN::Inclrip31))
	{
	  unsigned id0 = (itemIx - unsigned(CN::Inclrip0)) * bitsPerItem;
	  for (unsigned bitIx = 0; bitIx < bitsPerItem; ++bitIx)
	    if ((value >> bitIx) & 1)
	      tryClearIp(id0 + bitIx);
	  return true;
	}
      else if (itemIx == unsigned(CN::Clripnum))
	{
	  tryClearIp(value);  // Value is the interrupt id.
	  return true;
	}

      csrs_.at(itemIx).write(value);

      // Writing sourcecfg may change a source status. Cache status.
      if (itemIx >= unsigned(CN::Sourcecfg1) and
	  itemIx <= unsigned(CN::Sourcecfg1023))
	{
	  unsigned id = itemIx - unsigned(CN::Sourcecfg1) + 1;
	  unsigned ix = id / bitsPerItem;
	  unsigned bitIx = id % bitsPerItem;
	  bool flag = isActive(id);
	  CsrValue mask = CsrValue(1) << bitIx;
	  active_.at(ix) = flag ? active_.at(ix) | mask : active_.at(ix) & ~mask;
	  inverted_.at(ix) = isInverted(id);
	  if (flag)
	    assert(0 && "Evalute source for interrupt delivery");
	}

      return true;
    }

  return writeIdc(addr, size, value);
}


bool
Domain::readIdc(uint64_t addr, unsigned size, uint64_t& value)
{
  value = 0;

  if (not hasIdc_)
    return false;

  // Check size and alignment.
  unsigned reqSize = sizeof(CsrValue);  // Required size.
  if (size != reqSize or (addr & (reqSize - 1)) != 0)
    return false;

  uint64_t idcIndex = (addr - (addr_ + IdcOffset)) / sizeof(Idc);
  if (idcIndex >= idcs_.size())
    return false;

  Idc& idc = idcs_.at(idcIndex);
  size_t idcItemCount = sizeof(idc) / sizeof(idc.idelivery_);
  uint64_t itemIx = (addr - (addr_ + IdcOffset)) / reqSize;
  size_t idcItemIx = itemIx % idcItemCount;
  switch (idcItemIx)
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
      if (value == 0)
	idc.iforce_ = 0;
      else
	tryClearIp(value);
      break;

    default :
      break;
    }

  return true;
}


bool
Domain::writeIdc(uint64_t addr, unsigned size, uint64_t value)
{
  if (not hasIdc_)
    return false;

  // Check size and alignment.
  unsigned reqSize = sizeof(CsrValue);  // Required size.
  if (size != reqSize or (addr & (reqSize - 1)) != 0)
    return false;

  uint64_t idcIndex = (addr - (addr_ + IdcOffset)) / sizeof(Idc);
  if (idcIndex >= idcs_.size())
    return false;

  Idc& idc = idcs_.at(idcIndex);
  size_t idcItemCount = sizeof(idc) / sizeof(idc.idelivery_);
  uint64_t itemIx = (addr - (addr_ + IdcOffset)) / reqSize;
  size_t idcItemIx = itemIx % idcItemCount;
  switch (idcItemIx)
    {
    case 0  :
      idc.idelivery_  = value & 1;
      break;

    case 1  :
      idc.iforce_ = value & 1;
      break;

    case 2  :
      idc.ithreshold_ = value & ((1 << ipriolen_) - 1);
      break;

    case 3  :
      break;  // topi is not writeable

    case 4  :
      break;  // claimi is not writeable

    default :
      break;
    }
  return true;
}


void
Domain::defineCsrs()
{
  using CN = CsrNumber;

  csrs_.resize(size_t(CN::Target1023) + 1);

  CsrValue allOnes = ~CsrValue(0);

  CsrValue mask = Domaincfg::mask();
  CsrValue reset = 0x80000000;
  csrAt(CN::Domaincfg) = DomainCsr("domaincfg", CN::Domaincfg, reset, mask);

  reset = 0;
  std::string base = "sourcecfg";
  for (unsigned ix = 1; ix <= 1023; ++ix)
    {
      mask = ix < interruptCount_ ? Sourcecfg::nonDelegatedMask() : 0;
      std::string name = base + std::to_string(ix);
      CN cn{unsigned(CN::Sourcecfg1) + ix - 1};
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }

  reset = 0;
  csrAt(CN::Mmsiaddrcfg) = DomainCsr("mmsiaddrcfg", CN::Mmsiaddrcfg, reset, allOnes);
  mask = Mmsiaddrcfgh::mask();
  csrAt(CN::Mmsiaddrcfgh) = DomainCsr("mmsiaddrcfgh", CN::Mmsiaddrcfgh, reset, mask);

  csrAt(CN::Smsiaddrcfg) = DomainCsr("smsiaddrcfg", CN::Smsiaddrcfg, reset, allOnes);
  mask = Smsiaddrcfgh::mask();
  csrAt(CN::Smsiaddrcfgh) = DomainCsr("smsiaddrcfgh", CN::Smsiaddrcfgh, reset, mask);

  reset = 0;
  base = "setip";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      mask = ix == 0 ? ~CsrValue(1) : ~CsrValue(0);
      std::string name = base + std::to_string(ix);
      CN cn = advance(CN::Setip0, ix);
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }
  csrAt(CN::Setipnum) = DomainCsr("setipnum", CN::Setipnum, reset, allOnes);

  base = "in_clrip";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      mask = ix == 0 ? ~CsrValue(1) : ~CsrValue(0);
      std::string name = base + std::to_string(ix);
      CN cn = advance(CN::Inclrip0, + ix);
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }
  csrAt(CN::Clripnum) = DomainCsr("clripnum", CN::Clripnum, reset, allOnes);

  base = "setie";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      mask = ix == 0 ? ~CsrValue(1) : ~CsrValue(0);
      std::string name = base + std::to_string(ix);
      CN cn= advance(CN::Setie0, + ix);
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }
  csrAt(CN::Setienum) = DomainCsr("setienum", CN::Setienum, reset, allOnes);

  base = "clrie";
  for (unsigned ix = 0; ix <= 31; ++ix)
    {
      mask = ix == 0 ? ~CsrValue(1) : ~CsrValue(0);
      std::string name = base + std::to_string(ix);
      CN cn = advance(CN::Clrie0, + ix);
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }
  csrAt(CN::Clrienum) = DomainCsr("clrienum", CN::Clrienum, 0, allOnes);

  // Clear mask bits corresponding to non-implemented sources.
  unsigned bitsPerItem = sizeof(CsrValue)*8;
  unsigned missingIx = (interruptCount_) / bitsPerItem;
  for (unsigned ix = missingIx; ix <= 31; ++ix)
    {
      mask = 0;
      if (ix * bitsPerItem < interruptCount_)
	{
	  // Interrupt count is not a multiple of bitsPerItem. Clear
	  // upper part of item.
	  unsigned count = bitsPerItem - (interruptCount_ % bitsPerItem);
	  mask = ((~mask) << count) >> count;
	}

      CN cn = advance(CN::Setip0, ix);
      csrAt(cn).setMask(0);
      cn = advance(CN::Inclrip0, ix);
      csrAt(cn).setMask(0);
      cn = advance(CN::Setie0, ix);
      csrAt(cn).setMask(0);
      cn = advance(CN::Clrie0, ix);
      csrAt(cn).setMask(0);
    }

  reset = 0;
  csrAt(CN::Setipnumle) = DomainCsr("setipnum_le", CN::Setipnumle, reset, allOnes);
  csrAt(CN::Setipnumbe) = DomainCsr("setipnum_be", CN::Setipnumbe, reset, allOnes);

  reset = 0;
  mask = Genmsi::mask();
  csrAt(CN::Genmsi) = DomainCsr("genmsi", CN::Genmsi, reset, mask);
  
  mask = 0;
  reset = 0;
  base = "target";
  for (unsigned ix = 1; ix <= 1023; ++ix)
    {
      mask = ix < interruptCount_ ? Target::mask() : 0;
      std::string name = base + std::to_string(ix);
      CN cn = advance(CN::Target1, ix - 1);
      csrAt(cn) = DomainCsr(name, cn, reset, mask);
    }
}


void
Domain::defineIdcs()
{
  idcs_.resize(hartCount_);
}


bool
Domain::setSourceState(unsigned id, bool state)
{
  if (id >= interruptCount_ or id == 0)
    return false;

  unsigned childIx = 0;
  if (isDelegated(id, childIx))
    return children_.at(childIx)->setSourceState(id, state);

  // Determine interrupt target and priority.
  using CN = CsrNumber;
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};

  SourceMode mode = sourceMode(id);

  // Determine value of interrupt pending.
  bool ip = (mode == SourceMode::Edge1 or mode == SourceMode::Level1) == state;

  // Set interrupt pending.
  return setInterruptPending(id, ip);
}


bool
Domain::isDelegated(unsigned id) const
{
  if (id >= interruptCount_ or id == 0)
    return false;

  using CN = CsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  // Check if source is delegated.
  auto configVal = csrs_.at(unsigned(cn)).read();
  Sourcecfg sc{configVal};
  if (sc.d_)
    return true;
  return false;
}


bool
Domain::isDelegated(unsigned id, unsigned& childIx) const
{
  if (id >= interruptCount_ or id == 0)
    return false;

  using CN = CsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  // Check if source is delegated.
  auto configVal = csrs_.at(unsigned(cn)).read();
  Sourcecfg sc{configVal};
  if (sc.d_)
    {
      childIx = sc.child_;
      return true;
    }
  return false;
}


SourceMode
Domain::sourceMode(unsigned id) const
{
  if (id >= interruptCount_ or id == 0)
    return SourceMode::Inactive;

  using CN = CsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  // Check if source is delegated.
  auto configVal = csrs_.at(unsigned(cn)).read();
  Sourcecfg sc{configVal};
  if (sc.d_)
    return SourceMode::Inactive;

  /// Least significant 3 bits encode the source mode.
  return SourceMode{configVal & 7};
}


bool
Domain::setInterruptPending(unsigned id, bool flag)
{
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return false;

  using CN = CsrNumber;

  CN cn = advance(CN::Setip0, id);  // Number of CSR containing interrupt pending bits.
  unsigned bitsPerItem = sizeof(CsrValue) * 8;
  unsigned bitIx = id % bitsPerItem;
  CsrValue mask = CsrValue(1) << bitIx;
  uint32_t value = csrAt(cn).read();
  bool prev = value & mask;
  if (prev == flag)
    return true;  // Value did not change.

  // Update interrupt pending bit.
  value &= ~mask;  // Clear bit.
  if (flag)
    value |= mask;

  // Determine interrupt target and priority.
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};
  unsigned prio =  target.prio_;
  unsigned hart = target.hart_;

  if (hasIdc_)
    {
      // Update top priority interrupt. Lower priority number wins.
      // For tie, lower source id wins
      auto& idc = idcs_.at(hart);
      IdcTopi topi{idc.topi_};
      unsigned topPrio = topi.prio_;
      if (flag)
	{
	  if (prio <= topPrio or (prio == topPrio and id < topi.id_))
	    {
	      topi.prio_ = prio;
	      topi.id_ = id;
	    }
	}
      else if (id == topi.id_)
	{
	  // Interrupt that used to determine our top id went away. Re-compute
	  // top id and priority in interrupt delivery control.
	  topi.prio_ = 0;
	  topi.id_ = 0;
	  for (unsigned iid = 1; iid < interruptCount_; ++iid)
	    {
	      CN ntc = advance(CN::Target1, iid - 1);
	      uint32_t targetVal = csrAt(ntc).read();
	      Target target{targetVal};
	      if (target.hart_ == hart)
		if (topi.prio_ == 0 or target.prio_ < topi.prio_)
		  {
		    topi.prio_ = target.prio_;
		    topi.id_ = iid;
		  }
	    }
	}

      idc.topi_ = topi.value_;  // Update IDC.
      if ((topi.prio_ < idc.ithreshold_ or idc.ithreshold_ == 0) and
	  idc.idelivery_ and deliveryFunc_)
	deliveryFunc_(hart, isMachinePrivilege());
    }
  else
    {
      // Deliver using IMSIC
      assert(0);
    }

  return true;
}


bool
Domain::trySetIp(unsigned id)
{
  if (id == 0 or id >= interruptCount_ or isDelegated(id))
    return false;

  using CN = CsrNumber;

  uint32_t dcVal = csrAt(CN::Domaincfg).read();
  Domaincfg dc{dcVal};

  SourceMode mode = sourceMode(id);

  if (mode == SourceMode::Level0 or mode == SourceMode::Level1)
    {
      if (not dc.dm_)
	return false;  // Cannot set by a write in direct delivery mode
    }

  return setInterruptPending(id, true);
}


bool
Domain::tryClearIp(unsigned id)
{
  if (id == 0 or id >= interruptCount_ or isDelegated(id))
    return false;

  using CN = CsrNumber;

  uint32_t dcVal = csrAt(CN::Domaincfg).read();
  Domaincfg dc{dcVal};

  SourceMode mode = sourceMode(id);

  if (mode == SourceMode::Level0 or mode == SourceMode::Level1)
    {
      if (not dc.dm_)
	return false;  // Cannot clear by a write in direct delivery mode
    }

  return setInterruptPending(id, false);
}
