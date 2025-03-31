// SPDX-FileCopyrightText: © 2025 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

int
main(int, char**)
{
  // Define an interrupt delivery callback
  auto callback = [] (unsigned hartIx, Privilege privilege, bool interState) -> bool {
    std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
              << (privilege == Machine ? "machine" : "supervisor")
              << " interrupt-state=" << (interState? "on" : "off") << '\n';
    return true;
  };

  // Define an IMSIC delivery callback
  auto imsicFunc = [] (uint64_t addr, uint32_t data) -> bool {
    std::cerr << "Imsic write addr=0x" << std::hex << addr << " value="
              << data << std::dec << '\n';
    return true;
  };

  // In this example, the domain configuration will be:
  // root (machine, MSI), harts: 0
  //  --> child (supervisor, direct), harts: 0
  //  --> child2 (machine), harts: 1
  //    --> child3 (supervisor, MSI), harts: 1
  // And the sources configuration will be:
  // source1: active in child   ; source mode of Level1 ; target hart 0 with priority 1
  // source2: active in root    ; source mode of Level1 ; target hart 1
  // source3: active in child3  ; source mode of Edge0  ; target hart 1

  unsigned hartCount = 2;
  unsigned interruptCount = 33;

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;

  DomainParams domain_params[] = {
      { "root",   std::nullopt, 0, addr,                domainSize, Machine,    {0} },
      { "child",  "root",       0, addr + domainSize,   domainSize, Supervisor, {0} },
      { "child2", "root",       1, addr + 2*domainSize, domainSize, Machine,    {1} },
      { "child3", "child2",     0, addr + 3*domainSize, domainSize, Supervisor, {1} },
  };

  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  auto child = root->child(0);
  auto child2 = root->child(1);
  auto child3 = child2->child(0);

  aplic.setDirectCallback(callback);
  aplic.setMsiCallback(imsicFunc);

  // Aplic creation done. Test APIs.

  // Configure root domain for IMSIC delivery. Enable interrupt in root domain.
  uint32_t value = root->readDomaincfg();
  Domaincfg dcfg{value};
  dcfg.fields.dm = 1;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);

  // Configure source 1 in root domain as delegated.
  Sourcecfg cfg1{0};
  cfg1.d1.d = true;
  cfg1.d1.child_index = 0;
  root->writeSourcecfg(1, cfg1.value);

  // Configure source 1 in child domain as Level1 (acive high).
  cfg1 = Sourcecfg{0};
  cfg1.d0.sm = Level1;
  child->writeSourcecfg(1, cfg1.value);

  // Configure source 2 in root domain as Level1 (acive high).
  Sourcecfg cfg2{0};
  cfg2.d0.sm = Level1;
  root->writeSourcecfg(2, cfg2.value);

  // Configure source 3 in root domain as delegated.
  Sourcecfg cfg3{0};
  cfg3.d1.d = true;
  cfg3.d1.child_index = 1;
  root->writeSourcecfg(3, cfg3.value);

  // Configure source 3 in child2 domain as delegated.
  cfg3.d1.child_index = 0;
  child2->writeSourcecfg(3, cfg3.value);

  // Configure source 3 in child3 domain as Edge0 (falling edge).
  cfg3.d0.d = false;
  cfg3.d0.sm = Edge0;
  child3->writeSourcecfg(3, cfg3.value);

  // Configure child domain for direct delivery. Enable interrupt in child domain.
  value = child->readDomaincfg();
  dcfg = Domaincfg{value};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1;
  child->writeDomaincfg(dcfg.value);

  // Configure child3 domain for IMSIC delivery. Enable interrupt in child3 domain.
  value = child3->readDomaincfg();
  dcfg = Domaincfg{value};
  dcfg.fields.dm = 1;
  dcfg.fields.ie = 1;
  child3->writeDomaincfg(dcfg.value);

  unsigned hart = 0;

  // 1. Enable interrupt for source 1 in child.
  std::cerr << "Enabling interrupt for source 1 in child\n";

  // 1.1. Write the source id to the child IE CSR. In a real system this
  //      would be a store which targets the APLIC.
  unsigned sourceIx = 1;
  child->writeSetienum(sourceIx);

  // 2. Make source1 target hart 0 with priority 1.

  // 2.1. Construct value for Target1 CSR. Value encodes hart and priority.
  Target tgt{0};
  tgt.dm0.hart_index = hart;
  tgt.dm0.iprio = 1;

  // 2.3. Write CSR. In real system this would be a store.
  child->writeTarget(1, tgt.value);

  // 3. Enable idelivery in IDC of hart 0.
  child->writeIdelivery(hart, true);

  // 4. Set ithreshold in IDC of hart 0.
  uint32_t threshold = 2;
  child->writeIthreshold(hart, threshold);

  // 5. Enable inetrrupt for source 2 in root.
  std::cerr << "Enabling interrupt for source 2 in root\n";
  sourceIx = 2;
  root->writeSetienum(sourceIx);

  // 6. Make source2 target hart 1 with effective interrupt id 7.
  tgt.dm1.hart_index = 1;
  tgt.dm1.eiid = 7;
  root->writeTarget(2, tgt.value);

  // 7. Enable idelivery in IDC of hart 1 in root.
  hart = 1;
  root->writeIdelivery(hart, true);

  // 8. Set interrupt threshold in IDC of hart 1.
  hart = 1; threshold = 2;
  root->writeIthreshold(hart, threshold);

  // 9. Chnage the state of source 3.
  aplic.setSourceState(3, true);

  // 10. Enable interrupt for source 3
  std::cerr << "Enabling interrupt for source 3 in child 3\n";
  sourceIx = 3;
  child3->writeSetienum(sourceIx);

  // 11. Make source3 target hart 1, guest 1 with effective interrupt id 8.
  tgt.dm1.hart_index = 1;
  tgt.dm1.eiid = 8;
  tgt.dm1.guest_index = 1;

  child3->writeTarget(3, tgt.value);

  // 12. Enable idelivery in IDC of hart 1.
  child3->writeIdelivery(hart, true);

  // 13. Set interrupt threshold in IDC of hart 1.
  threshold = 3;
  child3->writeIthreshold(hart, threshold);

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
  value = child->readTarget(2);
  std::cout << "target value: " << std::hex << value << std::endl;
  srccfg.d0.sm = Inactive;
  child->writeSourcecfg(2, srccfg.value);
  value = child->readTarget(2);
  std::cout << "target value: "    << std::hex << value << ". (This should be 0.)" << std::endl;

  value = child->readMmsiaddrcfg();
  std::cout << "mmsiaddrcfg  read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  value = child->readMmsiaddrcfgh();
  std::cout << "mmsiaddrcfgh read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  value = child->readSmsiaddrcfg();
  std::cout << "smsiaddrcfg  read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  value = child->readSmsiaddrcfgh();
  std::cout << "smsiaddrcfgh read value in child domain: " << std::hex << value << ". (This should be 0.)" << std::endl;
  return 0;
}
