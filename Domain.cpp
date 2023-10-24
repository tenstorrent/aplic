#include <iostream>
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

  CsrValue val = 0;
  bool ok = true;

  uint64_t ix = (addr - addr_) / reqSize;
  if (ix < csrs_.size())
    {
      val = csrs_.at(ix).read();

      // Hide MSIADDRCFG if locked or not root domain
      using CN = CsrNumber;
      if (ix >= uint64_t(CN::Mmsiaddrcfg) and ix <= uint64_t(CN::Smsiaddrcfgh))
	{
	  auto root = rootDomain();
	  bool rootLocked = (root->csrAt(CN::Mmsiaddrcfgh).read() >> 31) & 1;
	  if (rootLocked or not isRoot())
	    val = ix == uint64_t(CN::Mmsiaddrcfgh) ? 0x80000000 : 0;
	}
    }
  else
    ok = readIdc(addr, size, val);

  if (bigEndian())
    val = __builtin_bswap32(val);

  value = val;
  return ok;
}


bool
Domain::write(uint64_t addr, unsigned size, uint64_t value)
{
  using CN = CsrNumber;

  unsigned reqSize = sizeof(CsrValue);  // Required size.

  // Check size and alignment.
  if (size != reqSize or (addr & (reqSize - 1)) != 0)
    return false;

  if (addr < addr_ or addr - addr_ >= size_)
    return false;

  unsigned bitsPerItem = reqSize*8;

  CsrValue val = value;
  if (bigEndian())
    val = __builtin_bswap32(val);

  uint64_t itemIx = (addr - addr_) / reqSize;
  if (itemIx < csrs_.size())
    {
      if (itemIx >= uint64_t(CN::Mmsiaddrcfg) and itemIx <= uint64_t(CN::Smsiaddrcfgh))
	{
	  auto root = rootDomain();
	  bool rootLocked = (root->csrAt(CN::Mmsiaddrcfgh).read() >> 31) & 1;
	  if (rootLocked or not isRoot())
	    return true; // No effect if not root or if root is locked.
	}
      else if (itemIx >= uint64_t(CN::Setip0) and itemIx <= uint64_t(CN::Setip31))
	{
	  unsigned id0 = (itemIx - unsigned(CN::Setip0)) * bitsPerItem;
	  for (unsigned bitIx = 0; bitIx < bitsPerItem; ++bitIx)
	    if ((val >> bitIx) & 1)
	      trySetIp(id0 + bitIx);
	  return true;
	}
      else if (itemIx == uint64_t(CN::Setipnum))
	{
	  trySetIp(val);  // Value is the interrupt id.
	  return true;  // Setipnum CSR is not updated (read zero).
	}
      if (itemIx >= uint64_t(CN::Inclrip0) and itemIx <= uint64_t(CN::Inclrip31))
	{
	  unsigned id0 = (itemIx - unsigned(CN::Inclrip0)) * bitsPerItem;
	  for (unsigned bitIx = 0; bitIx < bitsPerItem; ++bitIx)
	    if ((val >> bitIx) & 1)
	      tryClearIp(id0 + bitIx);
	  return true;
	}
      else if (itemIx == uint64_t(CN::Clripnum))
	{
	  tryClearIp(val);  // Value is the interrupt id.
	  return true;
	}
      else if (itemIx == uint64_t(CN::Setipnumle))
	{
	  trySetIp(value);
	  return true;
	}
      else if (itemIx == uint64_t(CN::Setipnumbe))
	{
	  val = value;
	  val = __builtin_bswap32(val);
	  trySetIp(value);
	  return true;
	}
      else if (itemIx == uint64_t(CN::Setienum))
	{
	  val = value;
	  trySetIe(value);
	  return true;
	}
      else if (itemIx < uint64_t(CN::Sourcecfg1) or itemIx > uint64_t(CN::Sourcecfg1023))
	{
	  if (isLeaf() and Sourcecfg{val}.bits_.d_)
	    val = 0;  // Section 4.5.2 of spec: Attempt to set D in a leaf domain
	}

      csrs_.at(itemIx).write(val);

      // Writing sourcecfg may change a source status. Update enable and pending
      // bits.
      postSourcecfgWrite(itemIx);

      return true;
    }

  return writeIdc(addr, size, val);
}


bool
Domain::readIdc(uint64_t addr, unsigned size, CsrValue& value)
{
  value = 0;

  if (not directDelivery())
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
      if (value >= idc.ithreshold_ and idc.ithreshold_ != 0)
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
Domain::writeIdc(uint64_t addr, unsigned size, CsrValue value)
{
  if (not directDelivery())
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
  size_t idcItemIx = itemIx % idcItemCount;  // Index of field with IDC.
  switch (idcItemIx)
    {
    case 0  :
      idc.idelivery_  = value & 1;
      break;

    case 1  :
      {
	CsrValue dcfgVal = csrAt(CsrNumber::Domaincfg).read();
	Domaincfg dcfg{dcfgVal};

	idc.iforce_ = value & 1;
	if (idc.iforce_ and idc.topi_ == 0 and idc.idelivery_ and
	    interruptEnabled() and deliveryFunc_)
	  deliveryFunc_(idcIndex, isMachinePrivilege());
      }
      break;

    case 2  :
      idc.ithreshold_ = value & ((1 << ipriolen_) - 1);
      break;

    case 3  :
      break;  // topi is not writable

    case 4  :
      break;  // claimi is not writable

    default :
      break;
    }
  return true;
}


void
Domain::postSourcecfgWrite(unsigned csrn)
{
  using CN = CsrNumber;

  if (csrn < unsigned(CN::Sourcecfg1) or csrn > unsigned(CN::Sourcecfg1023))
    return;

  unsigned id = csrn - unsigned(CN::Sourcecfg1) + 1;
  bool flag = isActive(id);

  if (not flag)
    {
      // Clear interrupt enabled/pending bits if id is not active.
      writeIp(id, false);
      writeIe(id, false);
    }

  // Make ip/IE writable or read-only-zero.
  setIpWritable(id, flag);
  setIeWritable(id, flag);

  // Check delegation.
  unsigned childIx = 0;
  bool delegated = isDelegated(id, childIx);
  CsrValue cfgMask = delegated ? Sourcecfg::delegatedMask() : Sourcecfg::nonDelegatedMask();
  csrs_.at(csrn).setMask(cfgMask);
  if (childIx < children_.size())
    {
      auto child = children_.at(childIx);
      if (delegated)
	{
	  // Child Sourcecfg mask for given id is now writable.
	  child->csrs_.at(csrn).setMask(Sourcecfg::nonDelegatedMask());

	  // Parent Sourcecfg mask is now for delegated
	  csrs_.at(csrn).setMask(Sourcecfg::delegatedMask());
	}
      else
	{
	  // Child Sourcecfg mask for given id is now non-writable
	  child->csrs_.at(csrn).setMask(0);

	  // Parent Sourcecfg mask is now for non-delegated
	  csrs_.at(csrn).setMask(Sourcecfg::nonDelegatedMask());
	}

      // Interrupt pending and enabled now writable in child if delegated.
      child->setIeWritable(id, delegated);
      child->setIpWritable(id, delegated);

      // Interrupt pending and enabled now writable in parent if non-delegated.
      setIeWritable(id, not delegated);
      setIpWritable(id, not delegated);
    }

  std::cerr << "Evaluate source for interrupt delivery\n";
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
  for (unsigned ix = 1; ix < EndId; ++ix)
    {
      if (ix >= interruptCount_)
	mask = 0;
      else
	mask = isRoot() ? Sourcecfg::nonDelegatedMask() : 0;
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
      CN cn = advance(CN::Setie0, + ix);
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
  for (unsigned ix = 1; ix < EndId; ++ix)
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
Domain::
setSourceState(unsigned id, bool state)
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
  if (mode == SourceMode::Detached)
    return true;   // Detached input ignored

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
  Sourcecfg sc{csrs_.at(unsigned(cn)).read()};
  return sc.bits_.d_;
}


bool
Domain::isDelegated(unsigned id, unsigned& childIx) const
{
  if (id >= interruptCount_ or id == 0)
    return false;

  using CN = CsrNumber;
  CN cn = advance(CN::Sourcecfg1, id - 1);

  Sourcecfg sc{csrs_.at(unsigned(cn)).read()};
  if (sc.bits_.d_)
    childIx = sc.bits_.child_;
  return sc.bits_.d_;
}


SourceMode
Domain::sourceMode(unsigned id) const
{
  if (id == 0 or id >= interruptCount_ or isDelegated(id))
    return SourceMode::Inactive;

  using CN = CsrNumber;

  CN cn = advance(CN::Sourcecfg1, id - 1);
  Sourcecfg sc{csrs_.at(unsigned(cn)).read()};
  return SourceMode{sc.bits2_.sm_};
}


bool
Domain::setInterruptPending(unsigned id, bool flag)
{
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return false;

  using CN = CsrNumber;

  bool prev = readIp(id);
  if (prev == flag)
    return true;  // Value did not change.

  bool enabled = readIe(id);

  // Update interrupt pending bit.
  writeIp(id, flag);

  // Determine interrupt target and priority.
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};
  unsigned prio =  target.bits_.prio_;
  unsigned hart = target.bits_.hart_;

  if (directDelivery())
    {
      // Update top priority interrupt. Lower priority number wins.
      // For tie, lower source id wins
      auto& idc = idcs_.at(hart);
      IdcTopi topi{idc.topi_};
      unsigned topPrio = topi.bits_.prio_;
      if (flag and enabled)
	{
	  if (prio < topPrio or (prio == topPrio and id < topi.bits_.id_))
	    {
	      topi.bits_.prio_ = prio;
	      topi.bits_.id_ = id;
	    }
	}
      else if (id == topi.bits_.id_ and not flag)
	{
	  // Interrupt that used to determine our top id went away. Re-compute
	  // top id and priority in interrupt delivery control.
	  topi.bits_.prio_ = 0;
	  topi.bits_.id_ = 0;
	  for (unsigned iid = 1; iid < interruptCount_; ++iid)
	    {
	      CN ntc = advance(CN::Target1, iid - 1);
	      CsrValue targetVal = csrAt(ntc).read();
	      Target target{targetVal};
	      if (target.bits_.hart_ == hart)
		if (topi.bits_.prio_ == 0 or target.bits_.prio_ < topi.bits_.prio_)
		  {
		    topi.bits_.prio_ = target.bits_.prio_;
		    topi.bits_.id_ = iid;
		  }
	    }
	}

      idc.topi_ = topi.value_;  // Update IDC.

      CsrValue dcfgVal = csrAt(CN::Domaincfg).read();
      Domaincfg dcfg{dcfgVal};
      if ((topi.bits_.prio_ < idc.ithreshold_ or idc.ithreshold_ == 0) and
	  idc.idelivery_ and interruptEnabled() and deliveryFunc_)
	deliveryFunc_(hart, isMachinePrivilege());
    }
  else
    {
      // Deliver to IMSIC
      if (interruptEnabled() and memoryWrite_)
	{
	  uint64_t imsicAddr = imsicAddress(hart);
	  uint32_t eiid = target.mbits_.eiid_;
	  memoryWrite_(imsicAddr, sizeof(eiid), eiid);
	  writeIp(id, false);  // Clear interrupt pending.
	}
    }

  return true;
}


bool
Domain::setInterruptEnabled(unsigned id, bool flag)
{
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return false;

  using CN = CsrNumber;

  bool prev = readIe(id);
  if (prev == flag)
    return true;  // Value did not change.

  bool pending = readIp(id);

  // Update interrupt enabled bit.
  writeIe(id, flag);

  // Determine interrupt target and priority.
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};
  unsigned prio =  target.bits_.prio_;
  unsigned hart = target.bits_.hart_;

  if (directDelivery())
    {
      // Update top priority interrupt. Lower priority number wins.
      // For tie, lower source id wins
      auto& idc = idcs_.at(hart);
      IdcTopi topi{idc.topi_};
      unsigned topPrio = topi.bits_.prio_;
      if (flag and pending)
	{
	  if (prio <= topPrio or (prio == topPrio and id < topi.bits_.id_))
	    {
	      topi.bits_.prio_ = prio;
	      topi.bits_.id_ = id;
	    }
	}
      else if (id == topi.bits_.id_ and not flag)
	{
	  // Interrupt that used to determine our top id went away. Re-compute
	  // top id and priority in interrupt delivery control.
	  topi.bits_.prio_ = 0;
	  topi.bits_.id_ = 0;
	  for (unsigned iid = 1; iid < interruptCount_; ++iid)
	    {
	      CN ntc = advance(CN::Target1, iid - 1);
	      CsrValue targetVal = csrAt(ntc).read();
	      Target target{targetVal};
	      if (target.bits_.hart_ == hart)
		if (topi.bits_.prio_ == 0 or target.bits_.prio_ < topi.bits_.prio_)
		  {
		    topi.bits_.prio_ = target.bits_.prio_;
		    topi.bits_.id_ = iid;
		  }
	    }
	}

      idc.topi_ = topi.value_;  // Update IDC.

      CsrValue dcfgVal = csrAt(CN::Domaincfg).read();
      Domaincfg dcfg{dcfgVal};
      if ((topi.bits_.prio_ < idc.ithreshold_ or idc.ithreshold_ == 0) and
	  idc.idelivery_ and interruptEnabled() and deliveryFunc_)
	deliveryFunc_(hart, isMachinePrivilege());
    }
  else
    {
      // Deliver to IMSIC
      if (interruptEnabled() and memoryWrite_)
	{
	  uint64_t imsicAddr = imsicAddress(hart);
	  uint32_t eiid = target.mbits_.eiid_;
	  memoryWrite_(imsicAddr, sizeof(eiid), eiid);
	  writeIp(id, false);  // Clear interrupt pending.
	}
    }

  return true;
}


bool
Domain::trySetIp(unsigned id)
{
  if (not isActive(id))
    return false;

  SourceMode mode = sourceMode(id);
  if (mode == SourceMode::Level0 or mode == SourceMode::Level1)
    if (directDelivery())
      return false;  // Cannot set by a write in direct delivery mode

  return setInterruptPending(id, true);
}


bool
Domain::tryClearIp(unsigned id)
{
  if (not isActive(id))
    return false;

  SourceMode mode = sourceMode(id);
  if (mode == SourceMode::Level0 or mode == SourceMode::Level1)
    if (directDelivery())
      return false;  // Cannot clear by a write in direct delivery mode

  return setInterruptPending(id, false);
}


bool
Domain::trySetIe(unsigned id)
{
  if (not isActive(id))
    return false;
  return setInterruptEnabled(id, true);
}


bool
Domain::tryClearIe(unsigned id)
{
  if (not isActive(id))
    return false;
  return setInterruptEnabled(id, false);
}


uint64_t
Domain::imsicAddress(unsigned hartIx)
{
  using CN = CsrNumber;

  unsigned itemBits = sizeof(CsrValue) * 8;

  uint64_t addr = 0;

  auto root = rootDomain();
  Mmsiaddrcfgh cfgh{root->csrAt(CN::Mmsiaddrcfgh).read()};

  uint64_t gg = (hartIx >> cfgh.bits_.lhxw_) & ((1 << cfgh.bits_.hhxw_) - 1);
  uint64_t hh = hartIx & ((1 << cfgh.bits_.lhxw_) - 1);
  uint64_t hhxs = cfgh.bits_.hhxs_;

  if (isMachinePrivilege())
    {
      uint64_t low = root->csrAt(CN::Mmsiaddrcfg).read();
      addr = (uint64_t(cfgh.bits_.ppn_) << itemBits) | low;
      addr = (addr | (gg << (hhxs + 12)) | (hh << cfgh.bits_.lhxs_)) << 12;
    }
  else
    {
      CN ntc = advance(CN::Target1, hartIx - 1);
      Target target{root->csrAt(ntc).read()};
      uint64_t guest = target.mbits_.guest_;

      Smsiaddrcfgh scfgh{root->csrAt(CN::Smsiaddrcfgh).read()};
      uint64_t low = root->csrAt(CN::Smsiaddrcfg).read();
      addr = (uint64_t(scfgh.bits_.ppn_) << itemBits) | low;
      addr = (addr | (gg << (hhxs + 12)) | (hh << scfgh.bits_.lhxs_) | guest) << 12;
    }

  return addr;
}
