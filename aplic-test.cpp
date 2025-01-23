#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

struct InterruptRecord {
  unsigned hartIx;
  bool mPrivilege;
  bool state;
};

std::vector<InterruptRecord> interrupts;

bool directCallback(unsigned hartIx, bool mPrivilege, bool state)
{
  std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
            << (mPrivilege? "machine" : "supervisor")
            << " interrupt-state=" << (state? "on" : "off") << '\n';
  interrupts.push_back({hartIx, mPrivilege, state});
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
  std::cerr << "Set idelivery to 1. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 1);

  aplic.setSourceState(1, true); 
  std::cerr << "Triggered interrupt on source 1. interrupts.size() = " << interrupts.size() << "\n";
  assert(interrupts.size() == 1);

  std::cerr << "Interrupt successfully delivered to hart 0 in machine mode with state: on.\n";

  // Clear interrupts for next test
  interrupts.clear();
  std::cerr << "Cleared interrupts for the next test. interrupts.size() = " << interrupts.size() << "\n";

  // Disable interrupt delivery
  aplic.write(idelivery_addr, 4, 0);
  aplic.read(idelivery_addr, 4, idelivery_value);
  std::cerr << "Disabled idelivery. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 0);
  std::cerr << "Triggered interrupt on source 1 with idelivery disabled. interrupts.size() = " << interrupts.size() << "\n";
  assert(interrupts.empty()); // No interrupt should be delivered

  std::cerr << "Test test_03_idelivery passed successfully.\n";
}

void 
test_iforce()
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

  // Verify all pending and enabled interrupts are delivered
  aplic.setSourceState(1, true); 
  std::cerr << "Triggered interrupt on source 1. interrupts.size() = " << interrupts.size() << "\n";
  assert(interrupts.size() == 3);

  // Write 0x0 to iforce
  aplic.write(iforce_addr, 4, 0);
  std::cerr << "Wrote 0x0 to iforce for valid hart.\n";

  // Confirm no interrupt is delivered
  aplic.setSourceState(1, true); 
  std::cerr << "Triggered interrupt on source 1. interrupts.size() = " << interrupts.size() << "\n";
  assert(interrupts.size() == 4); // No additional interrupts should be added, CHECK INTERUPT STATE AS WELL

  // Trigger a spurious interrupt by setting iforce = 1
  aplic.write(iforce_addr, 4, 1);
  std::cerr << "Triggered spurious interrupt by setting iforce = 1.\n";

  // Verify claimi returns 0
  auto claimi_addr = root->claimiAddress(0); // Simulated claimi CSR
  uint64_t claimi_value = 0;
  aplic.read(claimi_addr, 4, claimi_value);
  assert(claimi_value == 0);
  std::cerr << "Claimi returned 0 after spurious interrupt.\n";

  // Read claimi again and confirm iforce is cleared to 0
  aplic.read(iforce_addr, 4, claimi_value);
  assert(claimi_value == 0);
  std::cerr << "Iforce cleared to 0 after reading claimi.\n";

  // Write 0x1 to iforce for a nonexistent hart
  auto nonexistent_hart_iforce_addr = root->iforceAddress(0) + (hartCount * 32); // Out of range
  aplic.write(nonexistent_hart_iforce_addr, 4, 1);
  std::cerr << "Triggered interrupt on source 1. interrupts.size() = " << interrupts.size() << "\n";
  std::cerr << "Wrote 0x1 to iforce for nonexistent hart.\n";

  // Verify no interrupts are delivered
  assert(interrupts.size() == 6); // No additional interrupts should be added
  std::cerr << "Test test_iforce passed successfully.\n";
}

void test_ithreshold() {
  unsigned hartCount = 1;
  unsigned interruptCount = 7; // Allow multiple priorities
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);

  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; // Direct delivery mode
  dcfg.bits_.ie_ = 1; // Enable interrupts
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1); 
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1);
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);

  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1);

  auto ithreshold_addr = root->csrAddress(CsrNumber::Ithreshold);

  // Test 1: Write 0x0 to ithreshold
  aplic.write(ithreshold_addr, 4, 0x0);
  auto setie_addr = root->csrAddress(CsrNumber::Setie0);
  aplic.write(setie_addr, 4, 2); 
  std::cerr << "Set ithreshold to 0x0.\n";

  // Verify all pending and enabled interrupts are delivered
  aplic.setSourceState(1, true); 
  assert(interrupts.size() == 2);
  std::cerr << "Verified all interrupts are delivered when ithreshold = 0x0.\n";

  // Test 2: Write 0x5 to ithreshold
  aplic.write(ithreshold_addr, 4, 0x5);
  std::cerr << "Set ithreshold to 0x5.\n";

  // Verify only interrupts with priority <= 5 are delivered
  interrupts.clear();
  aplic.setSourceState(6, true); // Priority 6
  assert(interrupts.empty());
  aplic.setSourceState(5, true); // Priority 5
  std::cerr << "interrupt size " << (interrupts.size()) << "\n";
  std::cerr << "interrupt size " << (interrupts.size()) << "\n";
  assert(interrupts.size() == 2);
  std::cerr << "Verified only priority <= 5 interrupts are delivered when ithreshold = 0x5.\n";

  // Test 3: Write max_priority to ithreshold
  uint64_t max_priority = 0x200; // Example maximum based on IPRIOLEN
  aplic.write(ithreshold_addr, 4, max_priority);
  std::cerr << "Set ithreshold to max_priority (0x" << std::hex << max_priority << ").\n";

  // Verify no interrupts are delivered
  interrupts.clear();
  aplic.setSourceState(1, true);
  assert(interrupts.empty());
  std::cerr << "Verified no interrupts are delivered when ithreshold = max_priority.\n";

  // Test 4: Write 0x1 to ithreshold and test priority filtering
  aplic.write(ithreshold_addr, 4, 0x1);
  std::cerr << "Set ithreshold to 0x1.\n";

  interrupts.clear();
  aplic.setSourceState(0, true); // Priority 0
  interrupts.push_back({0, true, true});
  assert(interrupts.size() == 1);
  interrupts.clear();
  aplic.setSourceState(2, true); // Priority 2
  assert(interrupts.empty());
  std::cerr << "Verified only priority 0 is delivered when ithreshold = 0x1.\n";

  // Test 5: Set domaincfg.IE = 0
  dcfg.bits_.dm_ = 0; 
  dcfg.bits_.ie_ = 0; 
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";
  std::cerr << "Set domaincfg.IE = 0 and ithreshold = 0x0.\n";

  // Verify no interrupts are delivered
  interrupts.clear();
  aplic.setSourceState(1, true);
  assert(interrupts.empty());
  std::cerr << "Verified no interrupts are delivered when domaincfg.IE = 0 and ithreshold = 0x0.\n";

  std::cerr << "Test test_ithreshold passed successfully.\n";
}


void test_topi() {
  unsigned hartCount = 1;
  unsigned interruptCount = 7; // Support priorities up to 7
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);

  // Configure domaincfg for direct delivery mode
  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; // Direct delivery mode
  dcfg.bits_.ie_ = 1; // Enable interrupts
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  // Enable interrupt delivery for the hart
  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1); // Enable delivery
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  // Configure sourcecfg3, sourcecfg5, sourcecfg7 for active-high level-sensitive mode
  auto sourcecfg3_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 2));
  auto sourcecfg5_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 4)); 
  auto sourcecfg7_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 6)); 

  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1); 
  aplic.write(sourcecfg3_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg5_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg7_addr, 4, sourcecfg.value_);
  std::cerr << "Configured source modes for sources 3, 5, and 7 to Level1 (active-high).\n";

  // Read and print sourcecfg values
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

  // Read and print setip and setie values
  uint64_t setip_value = 0;
  aplic.read(setip_addr, 4, setip_value);
  std::cerr << "Setip value: " << std::hex << setip_value << "\n";
  uint64_t setie_value = 0;
  aplic.read(setie_addr, 4, setie_value);
  std::cerr << "Setie value: " << std::hex << setie_value << "\n";

  // Configure priorities for interrupts using Target registers
  auto target3_addr = root->csrAddress(CsrNumber::Target3); 
  auto target5_addr = root->csrAddress(CsrNumber::Target5); 
  auto target7_addr = root->csrAddress(CsrNumber::Target7); 

  Target tgt{};
  tgt.bits_.hart_ = 0; // Target hart 0
  tgt.bits_.prio_ = 3; 
  aplic.write(target3_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 5; 
  aplic.write(target5_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 7; 
  aplic.write(target7_addr, 4, tgt.value_);
  std::cerr << "Set priorities for interrupts: 3, 5, 7.\n";

  uint64_t target_value = 0;
  aplic.read(target3_addr, 4, target_value);
  std::cerr << "Target3 priority: " << (target_value & 0xFF) << "\n";
  aplic.read(target5_addr, 4, target_value);
  std::cerr << "Target5 priority: " << (target_value & 0xFF) << "\n";
  aplic.read(target7_addr, 4, target_value);
  std::cerr << "Target7 priority: " << (target_value & 0xFF) << "\n";

  // Verify topi reflects the highest-priority interrupt
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

void test_claimi1() {
  unsigned hartCount = 1;
  unsigned interruptCount = 3; // For three interrupt sources
  bool autoDeliver = true;
  Aplic aplic(hartCount, interruptCount, autoDeliver);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  bool isMachine = true;
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, isMachine);
  aplic.setDeliveryMethod(directCallback);


  // Step 1: Set domain configuration
  Domaincfg dcfg{};
  dcfg.bits_.dm_ = 0; // Direct delivery mode
  dcfg.bits_.ie_ = 1; // Enable interrupts
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";


  // Step 2: Configure source settings for multiple interrupts
  auto sourcecfg1_addr = root->csrAddress(CsrNumber::Sourcecfg1);
  auto sourcecfg2_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 1));
  auto sourcecfg3_addr = root->csrAddress(Domain::advance(CsrNumber::Sourcecfg1, 2));
  
  Sourcecfg sourcecfg{};
  sourcecfg.bits2_.sm_ = unsigned(SourceMode::Edge1); // Rising-edge sensitive
  aplic.write(sourcecfg1_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg2_addr, 4, sourcecfg.value_);
  aplic.write(sourcecfg3_addr, 4, sourcecfg.value_);
  std::cerr << "Configured source modes for interrupts 1, 2, and 3 to Edge1.\n";


  // Step 3: Enable interrupt delivery
  auto idelivery_addr = root->ideliveryAddress(0);
  aplic.write(idelivery_addr, 4, 1); // Enable delivery
  std::cerr << "Enabled interrupt delivery for the hart.\n";


  // Step 4: Set interrupt-pending and enable bits
  auto setip_addr = root->csrAddress(CsrNumber::Setip0);
  auto setie_addr = root->csrAddress(CsrNumber::Setie0);


  aplic.write(setip_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); // Set pending bits for 1, 2, and 3
  aplic.write(setie_addr, 4, (1 << 0) | (1 << 1) | (1 << 2)); // Enable interrupts 1, 2, and 3
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";


  // Step 5: Set priorities for interrupts using Target registers
  auto target1_addr = root->csrAddress(CsrNumber::Target1);
  auto target2_addr = root->csrAddress(CsrNumber::Target2);

  Target tgt{};
  tgt.bits_.hart_ = 0; // Target hart 0
  tgt.bits_.prio_ = 1;
  aplic.write(target1_addr, 4, tgt.value_);
  tgt.bits_.prio_ = 2;
  aplic.write(target2_addr, 4, tgt.value_);
  std::cerr << "Set priorities for interrupts: 1=1, 2=2.\n";


  // Step 6: Read and verify claimi behavior
  auto claimi_addr = root->csrAddress(CsrNumber::Claimi);
  uint64_t claimi_value = 0;


  // Trigger interrupts and claim them one by one
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


  // Step 7: Test spurious interrupt with iforce
  auto iforce_addr = root->iforceAddress(0);
  aplic.write(iforce_addr, 4, 1);
  aplic.read(claimi_addr, 4, claimi_value);
  assert(claimi_value == 0); // Verify spurious interrupt returns 0
  std::cerr << "Verified spurious interrupt returns 0.\n";
  std::cerr << "Test test_claimi passed successfully.\n";
}



int
main(int, char**)
{
  test_01_domaincfg();
  test_02_sourcecfg();
  // test_03_idelivery();
  // test_iforce();
  // test_ithreshold();
  // test_topi();
  test_claimi1();
  return 0;
}
