#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

int
main(int, char**)
{
  unsigned hartCount = 2;
  unsigned interruptCount = 33;
  unsigned domainCount = 2;

  uint64_t addr = 0x1000000;
  uint64_t stride = 32*1024;
  Aplic aplic(addr, stride, hartCount, domainCount, interruptCount);

  bool isMachine = true;
  auto root = aplic.createDomain(nullptr, addr, isMachine);

  // Configure root domain for IMSIC delivery. Enable interrupt in root domain.
  uint64_t value = 0;
  root->read(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);
  Domaincfg dcfg{CsrValue(value)};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1;
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);


  isMachine = false;
  auto child = aplic.createDomain(root, addr + stride, isMachine);

  // Configure source 1 in root domain as delegated.
  Sourcecfg cfg1{0};
  cfg1.bits_.d_ = true;
  cfg1.bits_.child_ = 0;
  aplic.write(root->csrAddress(CsrNumber::Sourcecfg1), sizeof(CsrValue), cfg1.value_);

  // Configure source 1 in child domain as Level1 (acive high).
  cfg1 = Sourcecfg{0};
  cfg1.bits2_.sm_ = unsigned(SourceMode::Level1);
  aplic.write(child->csrAddress(CsrNumber::Sourcecfg1), sizeof(CsrValue), cfg1.value_);

  // Configure source 2 in root domain as Level1 (acive high).
  Sourcecfg cfg2{0};
  cfg2.bits2_.sm_ = unsigned(SourceMode::Level1);
  CsrNumber csrn = Domain::advance(CsrNumber::Sourcecfg1, 1);
  aplic.write(root->csrAddress(csrn), sizeof(CsrValue), cfg2.value_);

  // Configure child domain for direct delivery. Enable interrupt in child domain.
  value = 0;
  child->read(child->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);
  dcfg = Domaincfg{CsrValue(value)};
  dcfg.bits_.dm_ = 1;
  dcfg.bits_.ie_ = 1;
  child->write(child->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);

  // Define an interrupt delivery callback
  auto callback = [] (unsigned hartIx, bool machine, bool ip) -> bool {
    std::cerr << "Interrupt at hart " << hartIx << " privilege "
	      << (machine? "machine" : "supervisor")
	      << " interrupt " << ip << '\n';
    return true;
  };
  aplic.setDeliveryMethod(callback);

  // Define an IMSIC delivery callback
  auto imsicFunc = [] (uint64_t addr, unsigned /*size*/, uint64_t data) -> bool {
    std::cerr << "Imsic write addr=0x" << std::hex << addr << " value="
	      << data << std::dec << '\n';
    return true;
  };
  aplic.setImsicMethod(imsicFunc);


  // Enable inetrrupt for source 1.
  unsigned writeSize = 4, sourceIx = 1, hart = 0, threshold = 2;
  child->write(child->csrAddress(CsrNumber::Setienum), writeSize, sourceIx);

  // Make source1 target hart 0 with priority 1.
  Target tgt{0};
  tgt.bits_.hart_ = hart;
  tgt.bits_.prio_ = 1;
  child->write(child->csrAddress(CsrNumber::Target1), sizeof(CsrValue), tgt.value_);

  // Enable idelivery in IDC of hart 0.
  child->write(child->ideliveryAddress(hart), sizeof(CsrValue), true);

  // Set ithreshold in IDC of hart 0.
  child->write(child->ithresholdAddress(hart), sizeof(CsrValue), threshold);

  // Enable inetrrupt for source 2.
  sourceIx = 2, hart = 1, threshold = 2;
  root->write(root->csrAddress(CsrNumber::Setienum), writeSize, sourceIx);

  // Make source2 target hart 1 with effective interrupt id 7.
  tgt.bits_.hart_ = hart;
  tgt.mbits_.eiid_ = 7;
  
  CsrNumber tgtCsr = Domain::advance(CsrNumber::Target1, 1);
  root->write(root->csrAddress(tgtCsr), sizeof(CsrValue), tgt.value_);

  // Enable idelivery in IDC of hart 1.
  root->write(root->ideliveryAddress(hart), sizeof(CsrValue), true);

  // Set ithreshold in IDC of hart 1.
  root->write(root->ithresholdAddress(hart), sizeof(CsrValue), threshold);

  // Set the 1st source state to high.
  aplic.setSourceState(1, true);

  // Set the 1st source state to low.
  aplic.setSourceState(1, false);

  // Set the 2st source state to high.
  aplic.setSourceState(2, true);

  // Set the 2st source state to low.
  aplic.setSourceState(2, false);

  // Set the 2st source state to high.
  aplic.setSourceState(2, true);

  return 0;
}
