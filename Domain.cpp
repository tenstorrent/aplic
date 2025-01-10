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

  if (addr < addr_ or addr - addr_ >= size_)
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
      else if (ix == uint64_t(CN::Genmsi))
        {
          if (directDelivery())
            val = 0;
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
      else if (itemIx >= uint64_t(CN::Inclrip0) and itemIx <= uint64_t(CN::Inclrip31))
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
      else if (itemIx >= uint64_t(CN::Setie0) and itemIx <= uint64_t(CN::Setie31))
        {
          unsigned id = (itemIx - uint64_t(CN::Setie0)) * bitsPerItem;
          for (unsigned bitIx = 0; bitIx < bitsPerItem; bitIx++)
            if ((val >> bitIx) & 1)
              trySetIe(id + bitIx);
          return true;
        }
      else if (itemIx == uint64_t(CN::Setienum))
        {
          trySetIe(val);
          return true;
        }
      else if (itemIx >= uint64_t(CN::Clrie0) and itemIx <= uint64_t(CN::Clrie31))
        {
          unsigned id0 = (itemIx - uint64_t(CN::Clrie0)) * bitsPerItem;
          for (unsigned bitIx = 0; bitIx < bitsPerItem; bitIx++)
            if ((val >> bitIx) & 1)
              tryClearIe(id0 + bitIx);
          return true;
        }
      else if (itemIx == uint64_t(CN::Clrienum))
        {
          tryClearIe(val);
          return true;
        }
      else if (itemIx >= uint64_t(CN::Sourcecfg1) and itemIx <= uint64_t(CN::Sourcecfg1023))
        {
          if (isLeaf() and Sourcecfg{val}.bits_.d_)
            val = 0;  // Section 4.5.2 of spec: Attempt to set D in a leaf domain

          csrs_.at(itemIx).write(val);

          // Writing sourcecfg may change a source status. Update enable/pending bits.
          postSourcecfgWrite(itemIx);
          return true;
        }
      else if (itemIx >= uint64_t(CN::Target1) and itemIx <= uint64_t(CN::Target1023))
        {
          if (directDelivery())
            {
              Target tgt{val};
              if (tgt.bits_.prio_ == 0)
                tgt.bits_.prio_ = 1;   // Priority bits must not be zero.
              val = tgt.value_;
            }
        }
      else if (itemIx == uint64_t(CN::Genmsi))
        {
          if (not directDelivery() and imsicFunc_)
            {
              Genmsi genmsi{val};
              uint64_t imsicAddr = imsicAddress(genmsi.bits_.hart_);
              uint32_t eiid = genmsi.bits_.eiid_;
              imsicFunc_(imsicAddr, sizeof(eiid), eiid);
            }
        }

      csrs_.at(itemIx).write(val);

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

  // Check size.
  if (size != sizeof(CsrValue))
    return false;

  Idc::Field field;
  uint64_t idcIx = 0;
  Idc* idc = findIdc(addr, idcIx, field);
  if (not idc)
    return false;

  unsigned id;
  switch (field)
    {
    case Idc::Field::Idelivery :
      value = idc->idelivery_;
      break;

    case Idc::Field::Iforce :
      value = idc->iforce_;
      break;

    case Idc::Field::Ithreshold :
      value = idc->ithreshold_;
      break;

    case Idc::Field::Topi :
      value = idc->topi_;
      id = (value >> 16) & 0x3ff;
      if (id >= idc->ithreshold_ and idc->ithreshold_ != 0)
        value = 0;
      break;

    case Idc::Field::Claimi :
      readIdc(addr-4, size, value); // claimi has same value as topi
      id = (value >> 16) & 0x3ff;
      if (id == 0)
        {
          idc->iforce_ = 0;
          deliveryFunc_(idcIx, isMachinePrivilege(), false);
        }
      else
        tryClearIp(id);
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

  // Check size.
  if (size != sizeof(CsrValue))
    return false;

  Idc::Field field;
  uint64_t idcIx = 0;
  Idc* idc = findIdc(addr, idcIx, field);
  if (not idc)
    return false;

  switch (field)
    {
    case Idc::Field::Idelivery :
      idc->idelivery_  = value & 1;
      break;

    case Idc::Field::Iforce :
      {
        idc->iforce_ = value & 1;
        // TODO(paul): what if topi is not 0?
        CsrValue topi;
        readIdc(topiAddress(idcIx), sizeof(CsrValue), topi);
        if (idc->iforce_ and topi == 0 and idc->idelivery_ and
            interruptEnabled() and deliveryFunc_)
          deliveryFunc_(idcIx, isMachinePrivilege(), true);
      }
      break;

    case Idc::Field::Ithreshold :
      idc->ithreshold_ = value & ((1 << ipriolen_) - 1);
      break;

    case Idc::Field::Topi :
      break;  // topi is not writable

    case Idc::Field::Claimi :
      break;  // claimi is not writable

    default :
      break;  // reserved fields are not writable
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
  for (unsigned ix = 1; ix <= MaxId; ++ix)
    {
      if (ix > interruptCount_)
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
  unsigned missingIx = (interruptCount_+1) / bitsPerItem;
  for (unsigned ix = missingIx; ix <= 31; ++ix)
    {
      mask = 0;
      if (ix * bitsPerItem <= interruptCount_)
        {
          // Interrupt count is not a multiple of bitsPerItem. Clear
          // upper part of item.
          unsigned count = bitsPerItem - ((interruptCount_+1) % bitsPerItem);
          mask = (allOnes << count) >> count;
          if (ix == 0)
            mask &= ~1;
        }

      CN cn = advance(CN::Setip0, ix);
      csrAt(cn).setMask(mask);
      cn = advance(CN::Inclrip0, ix);
      csrAt(cn).setMask(mask);
      cn = advance(CN::Setie0, ix);
      csrAt(cn).setMask(mask);
      cn = advance(CN::Clrie0, ix);
      csrAt(cn).setMask(mask);
    }

  reset = 0;
  csrAt(CN::Setipnumle) = DomainCsr("setipnum_le", CN::Setipnumle, reset, allOnes);
  csrAt(CN::Setipnumbe) = DomainCsr("setipnum_be", CN::Setipnumbe, reset, allOnes);

  reset = 0;
  mask = Genmsi::mask();
  csrAt(CN::Genmsi) = DomainCsr("genmsi", CN::Genmsi, reset, mask);

  reset = 0;
  if (directDelivery())
    reset = 1;  // iprio bits in target cannot be zero in direct delivery
  base = "target";
  for (unsigned ix = 1; ix <= MaxId; ++ix)
    {
      mask = ix <= interruptCount_ ? Target::mask() : 0;
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
Domain::setSourceState(unsigned id, bool prev, bool state)
{
  if (id > interruptCount_ or id == 0)
    return false;

  unsigned childIx = 0;
  if (isDelegated(id, childIx))
    return children_.at(childIx)->setSourceState(id, prev, state);

  // Determine interrupt target and priority.
  using CN = CsrNumber;
  CN ntc = advance(CN::Target1, id - 1);  // Number of target CSR.
  auto targetVal = csrAt(ntc).read();
  Target target{targetVal};

  SourceMode mode = sourceMode(id);
  if (mode == SourceMode::Detached)
    return true;   // Detached input ignored

  // Determine value of interrupt pending.
  bool ip =  (mode == SourceMode::Level0) and not state;
  ip = ip or ((mode == SourceMode::Level1) and state);
  ip = ip or ((mode == SourceMode::Edge0) and isFalling(prev, state));
  ip = ip or ((mode == SourceMode::Edge1) and isRising(prev, state));

  // Set rectified input value in in_clrip.
  if (id > 0 and id <= interruptCount_)
    writeBit(id, CsrNumber::Inclrip0, ip);

  // Set interrupt pending.
  return setInterruptPending(id, ip);
}


bool
Domain::isDelegated(unsigned id) const
{
  if (id > interruptCount_ or id == 0)
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
  if (id > interruptCount_ or id == 0)
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
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return SourceMode::Inactive;

  using CN = CsrNumber;

  CN cn = advance(CN::Sourcecfg1, id - 1);
  Sourcecfg sc{csrs_.at(unsigned(cn)).read()};
  return SourceMode{sc.bits2_.sm_};
}


bool
Domain::setInterruptPending(unsigned id, bool pending)
{
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return false;

  bool prev = readIp(id);
  if (prev == pending)
    return true;  // Value did not change.

  bool enabled = readIe(id);

  // Update interrupt pending bit.
  writeIp(id, pending);

  bool ready = enabled and pending;
  deliverInterrupt(id, ready);
  return true;
}


/// Deliver/undeliver interrupt of given source to the associated hart. This is called
/// when a source status changes.
void
Domain::deliverInterrupt(unsigned id, bool ready)
{
  using CN = CsrNumber;

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
      if (ready)
        {
          if (topPrio == 0 or prio < topPrio or (prio == topPrio and id < topi.bits_.id_))
            {
              topi.bits_.prio_ = prio;
              topi.bits_.id_ = id;
            }
        }
      else if (id == topi.bits_.id_)
        {
          // Interrupt that used to determine our top id went away. Re-compute
          // top id and priority in interrupt delivery control.
          topi.bits_.prio_ = 0;
          topi.bits_.id_ = 0;
          for (unsigned iid = 1; iid <= interruptCount_; ++iid)
            {
              CN ntc = advance(CN::Target1, iid - 1);
              CsrValue targetVal = csrAt(ntc).read();
              Target target{targetVal};
              if (target.bits_.hart_ == hart and id != iid)
                if (isActive(iid) and readIe(iid) and readIp(iid))
                  if (topi.bits_.prio_ == 0 or target.bits_.prio_ < topi.bits_.prio_)
                    {
                      topi.bits_.prio_ = target.bits_.prio_;
                      topi.bits_.id_ = iid;
                    }
            }
        }

      idc.topi_ = topi.value_;  // Update IDC.

      if (topi.bits_.id_)
        {
          if ((topi.bits_.prio_ < idc.ithreshold_ or idc.ithreshold_ == 0) and
              idc.idelivery_ and interruptEnabled() and deliveryFunc_)
            deliveryFunc_(hart, isMachinePrivilege(), true);
        }
      else
        deliveryFunc_(hart, isMachinePrivilege(), false);
    }
  else
    {
      // Deliver to IMSIC
      if (ready and interruptEnabled() and imsicFunc_)
        {
          uint64_t imsicAddr = imsicAddress(hart);
          uint32_t eiid = target.mbits_.eiid_;
          imsicFunc_(imsicAddr, sizeof(eiid), eiid);
          writeIp(id, false);  // Clear interrupt pending.
        }
    }
}


bool
Domain::setInterruptEnabled(unsigned id, bool enabled)
{
  if (id == 0 or id > interruptCount_ or isDelegated(id))
    return false;

  bool prev = readIe(id);
  if (prev == enabled)
    return true;  // Value did not change.

  bool pending = readIp(id);

  // Update interrupt enabled bit.
  writeIe(id, enabled);

  bool ready = enabled and pending;
  deliverInterrupt(id, ready);
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
