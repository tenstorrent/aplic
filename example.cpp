#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

int
main(int, char**)
{
  // Define an interrupt delivery callback
  auto callback = [] (unsigned hartIx, bool mPrivilege, bool interState) -> bool {
    std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
              << (mPrivilege? "machine" : "supervisor")
              << " interrupt-state=" << (interState? "on" : "off") << '\n';
    return true;
  };

  // Define an IMSIC delivery callback
  auto imsicFunc = [] (uint64_t addr, unsigned /*size*/, uint64_t data) -> bool {
    std::cerr << "Imsic write addr=0x" << std::hex << addr << " value="
              << data << std::dec << '\n';
    return true;
  };

  unsigned hartCount = 2;
  unsigned interruptCount = 33;

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  Aplic aplic(hartCount, interruptCount, true);

  aplic.setDeliveryMethod(callback);
  aplic.setImsicMethod(imsicFunc);

  // root
  //  --> child
  //  --> child2
  //    --> child3

  // Create root and child domains.
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);

  isMachine = false;
  auto child = aplic.createDomain("child", root, addr + domainSize, domainSize, isMachine);
  isMachine = true;
  auto child2 = aplic.createDomain("child2", root, addr + 2*domainSize, domainSize, isMachine);
  isMachine = false;
  auto child3 = aplic.createDomain("child3", child2, addr + 3*domainSize, domainSize, isMachine);

  // Aplic creation done. Test APIs.

  // Configure root domain for IMSIC delivery. Enable interrupt in root domain.
  uint64_t value = 0;
  root->read(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);
  Domaincfg dcfg{CsrValue(value)};
  dcfg.bits_.dm_ = 1;
  dcfg.bits_.ie_ = 1;
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);

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

  // Configure source 3 in root domain as delegated.
  Sourcecfg cfg3{0};
  cfg3.bits_.d_ = true;
  cfg3.bits_.child_ = 1;
  csrn = Domain::advance(CsrNumber::Sourcecfg1, 2);
  aplic.write(root->csrAddress(csrn), sizeof(CsrValue), cfg3.value_);

  // Configure source 3 in child2 domain as delegated.
  cfg3.bits_.child_ = 0;
  aplic.write(child2->csrAddress(csrn), sizeof(CsrValue), cfg3.value_);

  // Configure source 3 in child3 domain as Edge0 (falling edge).
  cfg3.bits2_.d_ = false;
  cfg3.bits2_.sm_ = unsigned(SourceMode::Edge0);
  aplic.write(child3->csrAddress(csrn), sizeof(CsrValue), cfg3.value_);

  // Configure child domain for direct delivery. Enable interrupt in child domain.
  value = 0;
  child->read(child->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);
  dcfg = Domaincfg{CsrValue(value)};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1;
  child->write(child->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);

  // Configure child3 domain for IMSIC delivery. Enable interrupt in child3 domain.
  value = 0;
  child3->read(child3->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);
  dcfg = Domaincfg{CsrValue(value)};
  dcfg.bits_.dm_ = 1;
  dcfg.bits_.ie_ = 1;
  child3->write(child3->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);

  unsigned hart = 0;

  // 1. Enable inetrrupt for source 1 in child.
  std::cerr << "Enabling interrupt for source 1 in child\n";

  // 1.1. Determine address corresponding to child IE CSR.
  uint64_t ieAddr = child->csrAddress(CsrNumber::Setienum);

  // 2.2. Write the source id to the child IE CSR. In a real system this
  //      would be a store which targets the APLIC.
  unsigned writeSize = 4, sourceIx = 1;
  aplic.write(ieAddr, writeSize, sourceIx);
  // Same as: child->write(ieAddr, writeSize, sourceIx);

  // 2. Make source1 target hart 0 with priority 1.

  // 2.1. Construct value for Target1 CSR. Value encodes hart and priority.
  Target tgt{0};
  tgt.bits_.hart_ = hart;
  tgt.bits_.prio_ = 1;
  CsrValue val = tgt.value_;

  // 2.2. Determine address of CSR corresponding to source 1.
  uint64_t tgt1Addr = child->csrAddress(CsrNumber::Target1);

  // 2.3. Write CSR. In real system this would be a store.
  aplic.write(tgt1Addr, sizeof(CsrValue), val);
  // Same as: child->write(tg1Addr, sizeof(CsrValue), val1);

  // 3. Enable idelivery in IDC of hart 0.
  aplic.write(child->ideliveryAddress(hart), sizeof(CsrValue), true);
  // Same as: child->write(child->ideliveryAddress(hart), sizeof(CsrValue), true);

  // 4. Set ithreshold in IDC of hart 0.
  CsrValue threshold = 2;
  aplic.write(child->ithresholdAddress(hart), sizeof(CsrValue), threshold);

  // 5. Enable inetrrupt for source 2 in root.
  std::cerr << "Enabling interrupt for source 2 in root\n";
  sourceIx = 2;
  aplic.write(root->csrAddress(CsrNumber::Setienum), writeSize, sourceIx);

  // 6. Make source2 target hart 1 with effective interrupt id 7.
  tgt.bits_.hart_ = 1;
  tgt.mbits_.eiid_ = 7;
  CsrNumber tgtCsr = Domain::advance(CsrNumber::Target1, 1);
  uint64_t tgt2Addr = root->csrAddress(tgtCsr);
  aplic.write(tgt2Addr, writeSize, tgt.value_);
  // Smae as: root->write(tgt2Addr, sizeof(CsrValue), tgt.value_);

  // 7. Enable idelivery in IDC of hart 1 in root.
  hart = 1;
  aplic.write(root->ideliveryAddress(hart), sizeof(CsrValue), true);

  // 8. Set interrupt threshold in IDC of hart 1.
  hart = 1; threshold = 2;
  aplic.write(root->ithresholdAddress(hart), sizeof(CsrValue), threshold);

  // 9. Chnage the state of source 3.
  aplic.setSourceState(3, true);

  // 10. Enable interrupt for source 3
  std::cerr << "Enabling interrupt for source 3 in child 3\n";
  sourceIx = 3;
  child3->write(child3->csrAddress(CsrNumber::Setienum), writeSize, sourceIx);

  // 11. Make source3 target hart 1, guest 1 with effective interrupt id 8.
  tgt.bits_.hart_ = 1;
  tgt.mbits_.eiid_ = 8;
  tgt.mbits_.guest_ = 1;

  auto tgtCsr3 = Domain::advance(CsrNumber::Target1, 2);
  aplic.write(child3->csrAddress(tgtCsr3), sizeof(CsrValue), tgt.value_);

  // 12. Enable idelivery in IDC of hart 1.
  aplic.write(child3->ideliveryAddress(hart), sizeof(CsrValue), true);

  // 13. Set interrupt threshold in IDC of hart 1.
  threshold = 3;
  aplic.write(child3->ithresholdAddress(hart), sizeof(CsrValue), threshold);

  // Set the 1st source state to high.
  std::cerr << "Source 1 high\n";
  aplic.setSourceState(1, true);

  // Set the 1st source state to low.
  std::cerr << "Source 1 low\n";
  aplic.setSourceState(1, false);

  // Set the 2nd source state to high.
  std::cerr << "Source 2 high\n";
  aplic.setSourceState(2, true);

  // Set the 2nd source state to low.
  std::cerr << "Source 2 low\n";
  aplic.setSourceState(2, false);

  // Set the 2nd source state to high.
  std::cerr << "Source 2 high\n";
  aplic.setSourceState(2, true);

  // Set the 3rd source state to low.
  std::cerr << "Source 3 low\n";
  aplic.setSourceState(3, false);

  // Set the 3rd source state to low.
  std::cerr << "Source 3 low\n";
  aplic.setSourceState(3, false);

  // Set the 3rd source state to high.
  std::cerr << "Source 3 high\n";
  aplic.setSourceState(3, true);


  // Target registers should be read-only zero for inactive sources.
  Sourcecfg srccfg{0};
  aplic.read (child->csrAddress(Domain::advance(CsrNumber::Target1, 1)), sizeof(CsrValue), value);
  std::cout << "target value: " << std::hex << value << std::endl;
  srccfg.bits2_.sm_ = unsigned(SourceMode::Inactive);
  aplic.write(child->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 1)), sizeof(CsrValue), srccfg.value_);
  aplic.read (child->csrAddress(Domain::advance(CsrNumber::Target1   , 1)), sizeof(CsrValue), value);
  std::cout << "target value: "    << std::hex << value << ". (This should be 0.)" << std::endl;

  value = 0;
  aplic.read(child->csrAddress(CsrNumber::Mmsiaddrcfg ), sizeof(CsrValue), value);
  std::cout << "mmsiaddrcfg  read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  aplic.read(child->csrAddress(CsrNumber::Mmsiaddrcfgh), sizeof(CsrValue), value);
  std::cout << "mmsiaddrcfgh read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  aplic.read(child->csrAddress(CsrNumber::Smsiaddrcfg ), sizeof(CsrValue), value);
  std::cout << "smsiaddrcfg  read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  aplic.read(child->csrAddress(CsrNumber::Smsiaddrcfgh), sizeof(CsrValue), value);
  std::cout << "smsiaddrcfgh read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  return 0;
}
