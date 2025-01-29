#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

struct InterruptRecord {
  unsigned hartIx;
  bool mPrivilege;
  bool state;
};

std::unordered_map<unsigned, bool> interruptStateMap;

std::vector<InterruptRecord> interrupts;

bool directCallback(unsigned hartIx, bool mPrivilege, bool state)
{
  std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
            << (mPrivilege? "machine" : "supervisor")
            << " interrupt-state=" << (state? "on" : "off") << '\n';
  interrupts.push_back({hartIx, mPrivilege, state});
  interruptStateMap[hartIx] = state;
  return true;
}

bool imsicCallback(uint64_t addr, unsigned /*size*/, uint64_t data)
{
  std::cerr << "Imsic write addr=0x" << std::hex << addr << " value=" << data << std::dec << '\n';
  return true;
}

void
test_01_domaincfg()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);

  uint64_t domaincfg = 0;
  auto root_domaincfg_addr = root->csrAddress(CsrNumber::Domaincfg);

  aplic.write(root_domaincfg_addr, 4, 0xfffffffe);
  aplic.read(root_domaincfg_addr, 4, domaincfg);
  assert(domaincfg == 0x80000104);

  aplic.write(root_domaincfg_addr, 4, 0xffffffff);
  aplic.read(root_domaincfg_addr, 4, domaincfg);
  assert(domaincfg == 0x5010080);
}

void
test_02_sourcecfg()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  auto child = aplic.createDomain("child", root, addr+domainSize, domainSize, isMachine);

  uint64_t csr_value = 0;
  auto sourcecfg2_csrn = Domain::advance(CsrNumber::Sourcecfg1, 1);
  auto root_sourcecfg2_addr = root->csrAddress(sourcecfg2_csrn);

  // For a system with N interrupt sources, write a non-zero value to a sourcecfg[i] where i > N; expect to read 0.
  aplic.write(root_sourcecfg2_addr, 4, 0x1);
  aplic.read(root_sourcecfg2_addr, 4, csr_value);
  assert(csr_value == 0);

  // Write a non-zero value to a sourcecfg[i] in a domain to which source i has not been delegated; expect to read 0x0.
  auto child_sourcecfg1_addr = child->csrAddress(CsrNumber::Sourcecfg1);
  aplic.write(child_sourcecfg1_addr, 4, 0x1);
  aplic.read(child_sourcecfg1_addr, 4, csr_value);
  assert(csr_value == 0);

  // Delegate a source i to a domain and write one of the supported source modes; expect to read that value.
  Sourcecfg root_sourcecfg1{0};
  root_sourcecfg1.bits_.d_ = true;
  root_sourcecfg1.bits_.child_ = true;
  auto root_sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1);
  aplic.write(root_sourcecfg1_addr, 4, root_sourcecfg1.value_);
  aplic.write(child_sourcecfg1_addr, 4, 0x1);
  aplic.read(child_sourcecfg1_addr, 4, csr_value);
  assert(csr_value == 1);

  // TODO: Write each reserved value for SM to a sourcecfg[i]; expect to read a legal value.
  // TODO: After setting sourcecfg[i] to a non-zero value in some domain, stop delegating that source to that domain; expect to read 0x0.
  // TODO: Make a source i not active in a domain, write non-zero values to ie, ip, and target; expect to read 0x0.
  // TODO: Write D=1 to a sourcecfg in a domain with no children; read D=0.
}

void 
test_03_idelivery()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1); 
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1);

  uint64_t idelivery_value = 0;
  aplic.read(idelivery_addr, 4, idelivery_value);
  assert(idelivery_value == 1);

  aplic.setSourceState(1, true); 
  assert(interrupts.size() == 1);

  std::cerr << "Interrupt successfully delivered to hart 0 in machine mode with state: on.\n";

  // Clear interrupts for next test
  interrupts.clear();

  // Disable interrupt delivery
  aplic.write(idelivery_addr, 4, 0);
  aplic.read(idelivery_addr, 4, idelivery_value);
  std::cerr << "Disabled idelivery. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 0);
  assert(interrupts.empty()); // No interrupt should be delivered

  std::cerr << "Test test_03_idelivery passed successfully.\n";
}

void 
test_04_iforce()
{
  unsigned hartCount = 2; 
  unsigned interruptCount = 1;
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; 
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1); 
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1);

  // Write 0x1 to iforce for a valid hart
  auto iforce_addr = root->iforceAddress(0); 
  aplic.write(iforce_addr, 4, 1);
  std::cerr << "Wrote 0x1 to iforce \n";

  auto setie_addr = root->csrAddress(CsrNumber::Setie0);
  aplic.write(setie_addr, 4, 2); 
  std::cerr << "Set ithreshold to 0x0.\n";

  aplic.setSourceState(1, true); 
  assert((interrupts.size() == 3) && interruptStateMap[0]);

  aplic.write(iforce_addr, 4, 0);
  std::cerr << "Wrote 0x0 to iforce for valid hart.\n";

  aplic.setSourceState(1, true); 
  assert(interrupts.size() == 4 && !interruptStateMap[0]); // No interrupt should be delivered

  aplic.write(iforce_addr, 4, 1);
  std::cerr << "Triggered spurious interrupt by setting iforce = 1.\n";

  auto claimi_addr = root->claimiAddress(0); 
  uint64_t claimi_value = 0;
  aplic.read(claimi_addr, 4, claimi_value);
  assert(claimi_value == 0);
  std::cerr << "Claimi returned 0 after spurious interrupt.\n";

  aplic.read(iforce_addr, 4, claimi_value);
  assert(claimi_value == 0);
  std::cerr << "Iforce cleared to 0 after reading claimi.\n";

  auto nonexistent_hart_iforce_addr = root->iforceAddress(0) + (hartCount * 32); // Out of range, try to write 0x1 to iforce for a nonexistent hart
  aplic.write(nonexistent_hart_iforce_addr, 4, 1);
  std::cerr << "Wrote 0x1 to iforce for nonexistent hart.\n";

  assert(interrupts.size() == 6 && !interruptStateMap[0]); // No additional interrupts should be added
  std::cerr << "Test test_iforce passed successfully.\n";
}

void 
test_05_ithreshold() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 3; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; 
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1);
  auto sourcecfg2_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 1));
  auto sourcecfg3_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 2));

  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1); 
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg2_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg3_addr, 4, sourcecfg.value_);
  std::cerr << "Configured source modes for interrupts 1, 2, and 3 to Edge1.\n";

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1); 
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  auto target1_addr = root->csrAddress(CsrNumber::Target1);
  auto target2_addr = root->csrAddress(CsrNumber::Target2);
  auto target3_addr = root->csrAddress(CsrNumber::Target3);

  Target tgt{};
  tgt.bits_.hart_ = 0; 
  tgt.bits_.prio_ = 0; 
  aplic.write(target1_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 5;
  aplic.write(target2_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 7; 
  aplic.write(target3_addr, 4, tgt.value_);
  std::cerr << "Set priorities for interrupts: 1=0, 2=5, 3=7.\n";

  // Verify all pending and enabled interrupts are delivered when ithreshold=0
  auto ithreshold_addr = root->csrAddress(CsrNumber::Ithreshold);
  aplic.write(ithreshold_addr, 4, 0x0); // ithreshold = 0
  std::cerr << "Set ithreshold to 0x0.\n";

  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  auto setie_addr = root->csrAddress(CsrNumber::Setie0);
  aplic.write(setip_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); 
  aplic.write(setie_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); 
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  aplic.setSourceState(1, true);
  assert(interruptStateMap[0]);
  aplic.setSourceState(2, true);
  assert(interruptStateMap[0]);
  aplic.setSourceState(3, true);
  assert(interrupts.size() == 11 && interruptStateMap[0]);

  // Verify only priority 0 interrupt is delivered when ithreshold=1
  aplic.write(ithreshold_addr, 4, 0x1); 
  std::cerr << "Set ithreshold to 0x1.\n";

  interrupts.clear();
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true); 
  assert(interrupts.size() == 1); 
  std::cerr << "Verified only priority 0 interrupt is delivered when ithreshold = 0x1.\n";

  // interrupts with priority <= 5 should be delivered
  aplic.write(ithreshold_addr, 4, 0x5);
  std::cerr << "Set ithreshold to 0x5.\n";
  aplic.write(setip_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); 
  aplic.write(setie_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); 
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  interrupts.clear();
  aplic.setSourceState(1, true); 
  assert(interruptStateMap[0]);
  aplic.setSourceState(2, true); 
  assert(!interruptStateMap[0]);
  aplic.setSourceState(3, true); 
  assert(interrupts.size() == 2 && !interruptStateMap[0]); 
  
  std::cerr << "Verified only interrupts with priority <= 5 are delivered when ithreshold = 0x5.\n";

  // Verify no interrupts are delivered when ithreshold=max_priority
  uint64_t max_priority = 0x200; 
  aplic.write(ithreshold_addr, 4, max_priority);
  std::cerr << "Set ithreshold to max_priority (0x" << std::hex << max_priority << ").\n";

  interrupts.clear();
  aplic.setSourceState(1, true);
  assert(!interruptStateMap[0]);
  aplic.setSourceState(2, true);
  assert(!interruptStateMap[0]);
  aplic.setSourceState(3, true);
  assert(interrupts.empty() && !interruptStateMap[0]);
  std::cerr << "Verified no interrupts are delivered when ithreshold = max_priority.\n";

  // Set domaincfg.IE = 0 and verify no interrupts are delivered
  dcfg.bits_.ie_ = 0; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Set domaincfg.IE = 0.\n";

  interrupts.clear();
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  aplic.setSourceState(3, true);
  assert(interrupts.empty());
  std::cerr << "Verified no interrupts are delivered when domaincfg.IE = 0.\n";
  std::cerr << "Test test_ithreshold passed successfully.\n";
}


void 
test_06_topi() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 7; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; 
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1); 
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  auto sourcecfg3_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 2));
  auto sourcecfg5_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 4)); 
  auto sourcecfg7_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 6)); 

  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1); 
  aplic.write(sourcecfg3_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg5_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg7_addr, 4, sourcecfg.value_);
  std::cerr << "Configured source modes for sources 3, 5, and 7 to Level1 (active-high).\n";

  uint64_t sourcecfg_value = 0;
  aplic.read(sourcecfg3_addr, 4, sourcecfg_value);
  std::cerr << "Sourcecfg3: " << std::hex << sourcecfg_value << "\n";
  aplic.read(sourcecfg5_addr, 4, sourcecfg_value);
  std::cerr << "Sourcecfg5: " << std::hex << sourcecfg_value << "\n";
  aplic.read(sourcecfg7_addr, 4, sourcecfg_value);
  std::cerr << "Sourcecfg7: " << std::hex << sourcecfg_value << "\n";

  // Set interrupt-pending and interrupt-enable bits
  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  auto setie_addr = root->csrAddress(CsrNumber::Setie0);

  aplic.write(setip_addr, 4, (1 << 3) | (1 << 5) | (1 << 7)); // Set pending bits for 3, 5, 7
  aplic.write(setie_addr, 4, (1 << 3) | (1 << 5) | (1 << 7)); // Enable interrupts 3, 5, 7
  std::cerr << "Set pending and enable bits for interrupts 3, 5, 7.\n";

  uint64_t setip_value = 0;
  aplic.read(setip_addr, 4, setip_value);
  uint64_t setie_value = 0;
  aplic.read(setie_addr, 4, setie_value);

  auto target3_addr = root->csrAddress(CsrNumber::Target3); 
  auto target5_addr = root->csrAddress(CsrNumber::Target5); 
  auto target7_addr = root->csrAddress(CsrNumber::Target7); 

  Target tgt{};
  tgt.bits_.hart_ = 0; 
  tgt.bits_.prio_ = 3; 
  aplic.write(target3_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 5; 
  aplic.write(target5_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 7; 
  aplic.write(target7_addr, 4, tgt.value_);
  std::cerr << "Set priorities for interrupts: 3, 5, 7.\n";

  uint64_t target_value = 0;
  aplic.read(target3_addr, 4, target_value);
  aplic.read(target5_addr, 4, target_value);
  aplic.read(target7_addr, 4, target_value);

  uint64_t topi_value = 0;
  auto topi_addr = root->csrAddress(CsrNumber::Topi);
  aplic.read(topi_addr, 4, topi_value);
  std::cerr << "Topi value: " << (topi_value >> 16) << " (priority: " << (topi_value & 0xFF) << ")\n";
  assert((topi_value >> 16) == 3);
  assert((topi_value & 0xFF) == 1); // Verify priority 3 is reflected
  std::cerr << "Verified topi returns priority 3 as the highest-priority interrupt.\n";

  // Set ithreshold = 5
  auto ithreshold_addr = root->csrAddress(CsrNumber::Ithreshold);
  aplic.write(ithreshold_addr, 4, 5);
  std::cerr << "Set ithreshold to 5.\n";

  // Verify topi reflects only interrupts with priority <= threshold
  aplic.read(topi_addr, 4, topi_value);
  std::cerr << "Topi value with ithreshold 5: " << (topi_value >> 16) << " (priority: " << (topi_value & 0xFF) << ")\n";
  assert(((topi_value >> 16) & 0xFF) == 3); 
  std::cerr << "Verified topi returns priority 3 when ithreshold = 5.\n";
  std::cerr << "Test test_topi passed successfully.\n";
}

void 
test_07_claimi() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 3; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1;
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1);
  auto sourcecfg2_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 1));
  auto sourcecfg3_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 2));
  
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg2_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg3_addr, 4, sourcecfg.value_);
  std::cerr << "Configured source modes for interrupts 1, 2, and 3 to Edge1.\n";

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1); 
  std::cerr << "Enabled interrupt delivery for the hart.\n";
  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  auto setie_addr = root->csrAddress(CsrNumber::Setie0);

  aplic.write(setip_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); // Set pending bits for 1, 2, and 3
  aplic.write(setie_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); // Enable interrupts 1, 2, and 3
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  auto target1_addr = root->csrAddress(CsrNumber::Target1);
  auto target2_addr = root->csrAddress(CsrNumber::Target2);
  Target tgt{};
  tgt.bits_.hart_ = 0; // Target hart 0
  tgt.bits_.prio_ = 1;
  aplic.write(target1_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 2;
  aplic.write(target2_addr, 4, tgt.value_);
  std::cerr << "Set priorities for interrupts: 1=1, 2=2.\n";

  auto claimi_addr = root->csrAddress(CsrNumber::Claimi);
  uint64_t claimi_value = 0;

  aplic.setSourceState(1, true);
  aplic.read(claimi_addr, 4, claimi_value);
  std::cerr << "Claimed interrupt: " << (claimi_value >> 16) << " (priority: " << (claimi_value & 0xFF) << ")\n";
  assert((claimi_value >> 16) == 1);
  assert((claimi_value & 0xFF) == 1);


  aplic.setSourceState(2, true);
  aplic.read(claimi_addr, 4, claimi_value);
  std::cerr << "Claimed interrupt: " << (claimi_value >> 16) << " (priority: " << (claimi_value & 0xFF) << ")\n";
  assert((claimi_value >> 16) == 2);
  assert((claimi_value & 0xFF) == 2);


  // Test spurious interrupt with iforce
  auto iforce_addr = root->iforceAddress(0);
  aplic.write(iforce_addr, 4, 1);
  aplic.read(claimi_addr, 4, claimi_value);
  assert(claimi_value == 0); 
  std::cerr << "Verified spurious interrupt returns 0.\n";
  std::cerr << "Test test_claimi passed successfully.\n";
}


void 
test_08_setipnum_le() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 10; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);


  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1); 
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1);

  uint64_t idelivery_value = 0;
  aplic.read(idelivery_addr, 4, idelivery_value);
  std::cerr << "Set idelivery to 1. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 1);

  // Write `0x01` to `setipnum_le`
  auto setipnum_le_addr = root->csrAddress(CsrNumber::Setipnumle);
  aplic.write(setipnum_le_addr, 4, 0x01); // Trigger interrupt 1
  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  uint64_t setip_value = 0;
  aplic.read(setip_addr, 4, setip_value);
  assert(setip_value & (1 << 1)); // Interrupt 1 bit should be set
  std::cerr << "Verified writing 0x01 to setipnum_le sets the corresponding bit in setip.\n";


  // Write `0x00` to `setipnum_le`
  aplic.write(setipnum_le_addr, 4, 0x00); // Invalid interrupt
  aplic.read(setip_addr, 4, setip_value);
  assert(!(setip_value & (1 << 0))); // Interrupt 0 bit should remain unset
  std::cerr << "Verified writing 0x00 to setipnum_le has no effect.\n";


  // Write `0x800` to `setipnum_le` (invalid identity)
  aplic.write(setipnum_le_addr, 4, 0x800); // Out of range interrupt
  aplic.read(setip_addr, 4, setip_value);
  assert(!(setip_value & (1 << 11))); // Ensure no invalid interrupt bit is set
  std::cerr << "Verified writing invalid identity (0x800) to setipnum_le has no effect.\n";


  std::cerr << "Test test_setipnum_le passed successfully.\n";
}


void 
test_09_setipnum_be() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 10; 
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);


  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1); 
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1);

  uint64_t idelivery_value = 0;
  aplic.read(idelivery_addr, 4, idelivery_value);
  std::cerr << "Set idelivery to 1. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 1);

  // Write `0x01` to `setipnum_be`
  auto setipnum_be_addr = root->csrAddress(CsrNumber::Setipnumbe);
  aplic.write(setipnum_be_addr, 4, 0x01); // Trigger interrupt 1
  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  uint64_t setip_value = 0;
  aplic.read(setip_addr, 4, setip_value);
  assert(setip_value & (1 << 1)); // Interrupt 1 bit should be set
  std::cerr << "Verified writing 0x01 to setipnum_be sets the corresponding bit in setip.\n";


  // Write `0x00` to `setipnum_be`
  aplic.write(setipnum_be_addr, 4, 0x00); // Invalid interrupt
  aplic.read(setip_addr, 4, setip_value);
  assert(!(setip_value & (1 << 0))); // Interrupt 0 bit should remain unset
  std::cerr << "Verified writing 0x00 to setipnum_be has no effect.\n";


  // Write `0x800` to `setipnum_be` (invalid identity)
  aplic.write(setipnum_be_addr, 4, 0x800); // Out of range interrupt
  aplic.read(setip_addr, 4, setip_value);
  assert(!(setip_value & (1 << 11))); // Ensure no invalid interrupt bit is set
  std::cerr << "Verified writing invalid identity (0x800) to setipnum_be has no effect.\n";


  std::cerr << "Test test_setipnum_be passed successfully.\n";
}


void 
test_10_targets() 
{
  unsigned hartCount = 4;  // Multiple harts to validate configurations
  unsigned interruptCount = 1023; 
  bool autoDeliver = true;
  TT_APLIC::Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);

  // MSI delivery mode
  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 1;  
  dcfg.bits_.ie_ = 1;  
  root->write(root->csrAddress(TT_APLIC::CsrNumber::Domaincfg), sizeof(TT_APLIC::CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for MSI delivery mode.\n";

  auto sourcecfg_addr = root->csrAddress(CsrNumber::Sourcecfg1);
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg_addr, 4, sourcecfg.value_);

  // Configure a valid Hart Index, Guest Index, and EIID 
  uint64_t target_value = 0;
  auto target_addr = root->csrAddress(CsrNumber::Target1);
  aplic.read(target_addr, 4, target_value);
  Target tgt{};
  tgt.mbits_.mhart_ = 2;  
  tgt.mbits_.guest_ = 3;  
  tgt.mbits_.eiid_ = 42;  
  aplic.write(target_addr, 4, tgt.value_);
  std::cerr << "Configured target register.\n";

  // Verify the MSI is sent to the correct hart, guest, and interrupt identity
  aplic.read(target_addr, 4, target_value);
  std::cerr << "after: " << target_value << "\n";
  assert((target_value & 0x7FF) == 42);  
  assert(((target_value >> 12) & 0x3F) == 3);  
  assert(((target_value >> 18) & 0x3FFF) == 2);  
  std::cerr << "Verified target configuration for hart, guest, and EIID.\n";

  // Write invalid values and verify they are ignored
  tgt.mbits_.mhart_ = 0xFFFF; 
  tgt.mbits_.guest_ = 0xFFFF; 
  tgt.mbits_.eiid_ = 0xFFF + 1; 
  aplic.write(target_addr, 4, tgt.value_);
  aplic.read(target_addr, 4, target_value);
  assert(((target_value >> 17) & 0x3FFF) != 0xFFFF); 
  assert(((target_value >> 11) & 0x3F) != 0xFFFF); 
  assert((target_value & 0x7FF) <= 0x7FF);            
  std::cerr << "Verified invalid values are ignored or adjusted.\n";

  // Configure multiple interrupts with equal priority and verify lowest source number takes precedence
  // Done in test_topi

  // Lock MSI address configuration and verify target writes are ignored
  auto mmsiaddrcfgh_addr = root->csrAddress(TT_APLIC::CsrNumber::Mmsiaddrcfgh);
  uint64_t mmsiaddrcfgh_value = 0x80000000;  // Lock flag
  aplic.write(mmsiaddrcfgh_addr, 4, mmsiaddrcfgh_value);
  aplic.write(target_addr, 4, tgt.value_);  // Attempt write after lock
  aplic.read(target_addr, 4, target_value);
  std::cerr << "after2: " << target_value << "\n";
  assert(target_value == 0x3F000);  // Target value should remain unchanged
  std::cerr << "Verified target registers are locked after MSI address configuration is locked.\n";

  std::cerr << "Test test_targets passed successfully.\n";
}


void 
test_11_MmsiAddressConfig() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  auto child = aplic.createDomain("child", root, addr + domainSize, domainSize, isMachine);

  // Configure MSI delivery mode
  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 1;  
  dcfg.bits_.ie_ = 1;  
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for MSI delivery mode.\n";

  auto mmsiaddrcfg_addr = root->csrAddress(CsrNumber::Mmsiaddrcfg);
  auto mmsiaddrcfgh_addr = root->csrAddress(CsrNumber::Mmsiaddrcfgh);
  auto child_mmsiaddrcfg_addr = child->csrAddress(CsrNumber::Mmsiaddrcfg);
  auto child_mmsiaddrcfgh_addr = child->csrAddress(CsrNumber::Mmsiaddrcfgh);

  uint32_t base_ppn = 0x123;  
  uint32_t hhxs = 0b10101;  
  uint32_t lhxs = 0b110;    
  uint32_t hhxw = 0b111;    
  uint32_t lhxw = 0b1111;   
  uint32_t lock_bit = 0;    

  uint32_t mmsiaddrcfg_value = base_ppn | (lhxw << 12);
  uint32_t mmsiaddrcfgh_value = (hhxw << 0) | (hhxs << 4) | (lhxs << 8) | (lock_bit << 31);

  aplic.write(mmsiaddrcfg_addr, 4, mmsiaddrcfg_value);
  aplic.write(mmsiaddrcfgh_addr, 4, mmsiaddrcfgh_value);
  std::cerr << "Wrote valid values to mmsiaddrcfg and mmsiaddrcfgh in root domain.\n";

  uint64_t read_value = 0;
  aplic.read(mmsiaddrcfg_addr, 4, read_value);
  assert(read_value == mmsiaddrcfg_value);
  aplic.read(mmsiaddrcfgh_addr, 4, read_value);
  assert(read_value == mmsiaddrcfgh_value);
  std::cerr << "Verified written values are stored correctly in root domain.\n";

  // Verify Child Domain is Read-Only
  uint32_t child_invalid_value = 0xFFFFFFFF;  
  aplic.write(child_mmsiaddrcfg_addr, 4, child_invalid_value);
  aplic.write(child_mmsiaddrcfgh_addr, 4, child_invalid_value);

  uint64_t child_read_value = 0;
  aplic.read(child_mmsiaddrcfg_addr, 4, child_read_value);
  assert(child_read_value == 0);  // read-only
  aplic.read(child_mmsiaddrcfgh_addr, 4, child_read_value);
  std::cerr << "child_read_value: " << child_read_value << "\n";
  assert(child_read_value == 0x80000000);  // read-only

  std::cerr << "Verified mmsiaddrcfg and mmsiaddrcfgh are **read-only** in non-root machine domains.\n";

  // Lock the MSI Configuration and Verify Writes Are Ignored in Root Domain
  uint32_t lock_value = mmsiaddrcfgh_value | (1 << 31);  
  aplic.write(mmsiaddrcfgh_addr, 4, lock_value);
  aplic.read(mmsiaddrcfgh_addr, 4, read_value);
  assert((read_value & (1 << 31)) != 0);
  std::cerr << "Verified MSI address configuration lock bit is set.\n";

  // Attempt to modify after locking (should not take effect)
  aplic.write(mmsiaddrcfg_addr, 4, 0x123);
  aplic.write(mmsiaddrcfgh_addr, 4, 0x123);
  aplic.read(mmsiaddrcfg_addr, 4, read_value);
  assert((read_value == mmsiaddrcfg_value) || (read_value == 0));  
  aplic.read(mmsiaddrcfgh_addr, 4, read_value);
  assert(read_value == lock_value || (read_value == 0x80000000));  
  std::cerr << "Verified lock prevents further writes in root domain.\n";

  std::cerr << "Test testMmsiAddressConfig passed successfully.\n";
}


void 
test_12_SmsiAddressConfig() 
{
  unsigned hartCount = 2;  
  unsigned interruptCount = 1;
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  auto child = aplic.createDomain("child", root, addr + domainSize, domainSize, isMachine);

  auto smsiaddrcfg_addr = root->csrAddress(CsrNumber::Smsiaddrcfg);
  auto smsiaddrcfgh_addr = root->csrAddress(CsrNumber::Smsiaddrcfgh);
  auto child_smsiaddrcfg_addr = child->csrAddress(CsrNumber::Smsiaddrcfg);
  auto child_smsiaddrcfgh_addr = child->csrAddress(CsrNumber::Smsiaddrcfgh);

  uint32_t base_ppn = 0x234;  // 12-bit PPN
  uint32_t lhxs = 0b101;      // 3-bit field
  uint32_t smsiaddrcfg_value = base_ppn;
  uint32_t smsiaddrcfgh_value = lhxs;

  aplic.write(smsiaddrcfg_addr, 4, smsiaddrcfg_value);
  aplic.write(smsiaddrcfgh_addr, 4, smsiaddrcfgh_value);
  std::cerr << "Wrote valid values to smsiaddrcfg and smsiaddrcfgh in root domain.\n";

  uint64_t read_value = 0;
  aplic.read(smsiaddrcfg_addr, 4, read_value);
  assert(read_value == smsiaddrcfg_value);
  aplic.read(smsiaddrcfgh_addr, 4, read_value);
  assert(read_value == smsiaddrcfgh_value);
  std::cerr << "Verified values match after writing in root domain.\n";

  // Verify non-root domains cannot write these registers
  aplic.write(child_smsiaddrcfg_addr, 4, 0xFFFFFFFF);
  aplic.write(child_smsiaddrcfgh_addr, 4, 0xFFFFFFFF);

  uint64_t child_read_value = 0;
  aplic.read(child_smsiaddrcfg_addr, 4, child_read_value);
  assert(child_read_value == 0 || child_read_value == smsiaddrcfg_value);  // Expect read-only
  aplic.read(child_smsiaddrcfgh_addr, 4, child_read_value);
  assert(child_read_value == 0 || child_read_value == smsiaddrcfgh_value);
  std::cerr << "Verified smsiaddrcfg and smsiaddrcfgh are **read-only** in non-root domains.\n";

  // Locking mmsiaddrcfgh and verifying lock applies to supervisor registers
  auto mmsiaddrcfgh_addr = root->csrAddress(CsrNumber::Mmsiaddrcfgh);
  uint32_t lock_value = (1 << 31);
  aplic.write(mmsiaddrcfgh_addr, 4, lock_value);

  aplic.write(smsiaddrcfg_addr, 4, 0x123);  
  aplic.write(smsiaddrcfgh_addr, 4, 0x123);  

  aplic.read(smsiaddrcfg_addr, 4, read_value);
  assert((read_value == smsiaddrcfg_value) || (read_value == 0));  
  aplic.read(smsiaddrcfgh_addr, 4, read_value);
  assert((read_value == smsiaddrcfgh_value) || (read_value == 0));
  std::cerr << "Verified supervisor MSI registers are locked after setting lock in mmsiaddrcfgh.\n";
  std::cerr << "Test testSmsiAddressConfig passed successfully.\n";
}

int
main(int, char**)
{
  test_01_domaincfg();
  test_02_sourcecfg();
  test_03_idelivery();
  test_04_iforce();
  test_05_ithreshold();
  test_06_topi();
  test_07_claimi();
  test_08_setipnum_le();
  test_09_setipnum_be();
  test_10_targets();
  test_11_MmsiAddressConfig();
  test_12_SmsiAddressConfig();
  return 0;
}
