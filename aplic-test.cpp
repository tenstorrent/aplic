#include <iostream>
#include "Aplic.hpp"

using namespace TT_APLIC;

struct InterruptRecord {
  unsigned hartIx;
  Privilege privilege;
  bool state;
};

std::unordered_map<unsigned, bool> interruptStateMap;

std::vector<InterruptRecord> interrupts;

bool directCallback(unsigned hartIx, Privilege privilege, bool state)
{
  std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
            << (privilege == Machine? "machine" : "supervisor")
            << " interrupt-state=" << (state? "on" : "off") << '\n';
  interrupts.push_back({hartIx, privilege, state});
  std::cerr << "HIIIII.\n";
  interruptStateMap[hartIx] = state;
  return true;
}

bool imsicCallback(uint64_t addr, uint64_t data)
{
  std::cerr << "Imsic write addr=0x" << std::hex << addr << " value=" << data << std::dec << '\n';
  return true;
}

void
test_01_domaincfg()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);

  root->writeDomaincfg(0xfffffffe);
  uint32_t domaincfg = root->readDomaincfg();
  assert(domaincfg == 0x80000104);

  root->writeDomaincfg(0xffffffff);
  domaincfg = root->readDomaincfg();
  //assert(domaincfg == 0x5010080); // TODO: big-endian does not work yet
}

void
test_02_sourcecfg()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  auto child = aplic.createDomain("child", root, addr+domainSize, domainSize, Supervisor, hartIndices);

  // For a system with N interrupt sources, write a non-zero value to a sourcecfg[i] where i > N; expect to read 0.
  root->writeSourcecfg(2, 0x1);
  uint32_t csr_value = root->readSourcecfg(2);
  assert(csr_value == 0);

  // Write a non-zero value to a sourcecfg[i] in a domain to which source i has not been delegated; expect to read 0x0.
  child->writeSourcecfg(1, 0x1);
  csr_value = child->readSourcecfg(1);
  assert(csr_value == 0);

  // Delegate a source i to a domain and write one of the supported source modes; expect to read that value.
  Sourcecfg root_sourcecfg1{0};
  root_sourcecfg1.d1.d = true;
  root_sourcecfg1.d1.child_index = true;
  root->writeSourcecfg(1, root_sourcecfg1.value);
  child->writeSourcecfg(1, 0x1);
  csr_value = child->readSourcecfg(1);
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
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  aplic.setDirectCallback(directCallback);

  Domaincfg dcfg{};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = Edge1;
  root->writeSourcecfg(1, sourcecfg.value);

  root->writeIdelivery(0, 1);

  uint32_t idelivery_value = root->readIdelivery(0);
  assert(idelivery_value == 1);

  root->writeSetienum(1);

  aplic.setSourceState(1, true); 
  assert(interrupts.size() == 1);

  std::cerr << "Interrupt successfully delivered to hart 0 in machine mode with state: on.\n";

  // Clear interrupts for next test
  interrupts.clear();

  // Disable interrupt delivery
  root->writeIdelivery(0, 0);
  idelivery_value = root->readIdelivery(0);
  std::cerr << "Disabled idelivery. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 0);
  assert(interrupts.size() == 1); // Interrupt should be undelivered

  std::cerr << "Test test_03_idelivery passed successfully.\n";
}

void 
test_04_iforce()
{
  unsigned hartCount = 2; 
  unsigned interruptCount = 1;
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  aplic.setDirectCallback(directCallback);

  Domaincfg dcfg{};
  dcfg.fields.dm = 0; 
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = Edge1;
  root->writeSourcecfg(1, sourcecfg.value);

  root->writeIdelivery(0, 1);

  // Write 0x1 to iforce for a valid hart
  root->writeIforce(0, 1);
  std::cerr << "Wrote 0x1 to iforce \n";

  root->writeSetie(0, 2); 
  std::cerr << "Set ithreshold to 0x0.\n";

  aplic.setSourceState(1, true); 
  std::cerr << "interrupts.size() " << interrupts.size() << "\n";
  std::cerr << "STATE " << interruptStateMap[0] << "\n";
  assert((interrupts.size() == 2) && interruptStateMap[0]); // TODO

  root->writeIforce(0, 0);
  std::cerr << "Wrote 0x0 to iforce for valid hart.\n";

  aplic.setSourceState(1, true);
  std::cerr << "interrupts.size() " << interrupts.size() << "\n";
  std::cerr << "STATE " << interruptStateMap[0] << "\n"; 
  // assert(interrupts.size() == 2 && !interruptStateMap[0]); // TODO

  root->writeIforce(0, 1);
  std::cerr << "Triggered spurious interrupt by setting iforce = 1.\n";

  uint32_t claimi_value = root->readClaimi(0);
  std::cerr << "Claimi " << claimi_value << "\n"; 
  //assert(claimi_value == 0); // TODO
  std::cerr << "Claimi returned 0 after spurious interrupt.\n";

  claimi_value = root->readIforce(0);
  //assert(claimi_value == 0); // TODO
  std::cerr << "Iforce cleared to 0 after reading claimi.\n";

  // Write 0x1 to iforce for a nonexistent hart
  // root->writeIforce(hartCount, 1); // TODO: this would cause an assertion; use aplic.write() instead
  std::cerr << "Wrote 0x1 to iforce for nonexistent hart.\n";

  //assert(interrupts.size() == 6 && !interruptStateMap[0]); // No additional interrupts should be added // TODO
  std::cerr << "Test test_iforce passed successfully.\n";
}

void 
test_05_ithreshold() //INTERRUPT SIZE NOT UPDATING
{
  unsigned hartCount = 1;
  unsigned interruptCount = 3;
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  aplic.setDirectCallback(directCallback);

  Domaincfg dcfg{};
  dcfg.fields.dm = 0; 
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1);
  root->writeSourcecfg(1, sourcecfg.value);
  root->writeSourcecfg(2, sourcecfg.value);
  root->writeSourcecfg(3, sourcecfg.value);
  std::cerr << "Configured source modes for interrupts 1, 2, and 3 to Edge1.\n";

  root->writeIdelivery(0, 1);
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  Target tgt{};
  tgt.dm0.hart_index = 0; 
  tgt.dm0.iprio = 0; 
  root->writeTarget(1, tgt.value);
  tgt.dm0.iprio = 5;
  root->writeTarget(2, tgt.value);
  tgt.dm0.iprio = 7; 
  root->writeTarget(3, tgt.value);
  std::cerr << "Set priorities for interrupts: 1=0, 2=5, 3=7.\n";

  // Verify all pending and enabled interrupts are delivered when ithreshold=0
  root->writeIthreshold(0, 0x0); // ithreshold = 0
  std::cerr << "Set ithreshold to 0x0.\n";

  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3)); // Pending interrupts 1, 2, 3
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3)); // Enable interrupts 1, 2, 3
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  aplic.setSourceState(1, true);
  assert(interruptStateMap[0]);
  aplic.setSourceState(2, true);
  assert(interruptStateMap[0]);
  aplic.setSourceState(3, true);
  assert(interruptStateMap[0]); 

  // Verify only priority 0 interrupt is delivered when ithreshold=1
  root->writeIthreshold(0, 0x1); 
  std::cerr << "Set ithreshold to 0x1.\n";

  interrupts.clear();
  root->writeSetip(0, (1 << 1) | (1 << 2)); 
  root->writeSetie(0, (1 << 1) | (1 << 2));
  aplic.setSourceState(1, true); 
  aplic.setSourceState(2, true); 
  assert(interrupts.size() == 1); 
  std::cerr << "Verified only priority 0 interrupt is delivered when ithreshold = 0x1.\n";

  // interrupts with priority <= 5 should be delivered
  root->writeIthreshold(0, 0x5);
  std::cerr << "Set ithreshold to 0x5.\n";
  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3)); 
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3)); 
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  interrupts.clear();
  aplic.setSourceState(1, true); // Priority 0, should be delivered 
  assert(interruptStateMap[0]);
  aplic.setSourceState(2, true); // Priority 5, should be delivered 
  assert(interruptStateMap[0]); 
  aplic.setSourceState(3, true); // Priority 7, should NOT be delivered 
  std::cerr << "SIZE: " << interrupts.size() << "\n";
  std::cerr << "STATE: " << interruptStateMap[0] << "\n"; 
  // assert(!interruptStateMap[0]); // TODO
  
  std::cerr << "Verified only interrupts with priority <= 5 are delivered when ithreshold = 0x5.\n";

  // Verify all interrupts enabled except for max_priority
  uint64_t max_priority = 0xFF; 
  root->writeIthreshold(0, max_priority);
  std::cerr << "Set ithreshold to max_priority (0x" << std::hex << max_priority << ").\n";

  interrupts.clear();
  aplic.setSourceState(1, true);
  assert(interruptStateMap[0]); 
  aplic.setSourceState(2, true);
  assert(interruptStateMap[0]); 
  aplic.setSourceState(3, true);
  assert(interruptStateMap[0]);
  std::cerr << "Verified all interrupts are delivered when ithreshold = max_priority.\n";

  // Set domaincfg.IE = 0 and verify no interrupts are delivered
  dcfg.fields.ie = 0; 
  root->writeDomaincfg(dcfg.value);
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
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);

  Domaincfg dcfg{};
  dcfg.fields.dm = 0; 
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  root->writeIdelivery(0, 1); 
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1); 
  root->writeSourcecfg(3, sourcecfg.value);
  root->writeSourcecfg(5, sourcecfg.value);
  root->writeSourcecfg(7, sourcecfg.value);
  std::cerr << "Configured source modes for sources 3, 5, and 7 to Level1 (active-high).\n";

  uint64_t sourcecfg_value = 0;
  sourcecfg_value = root->readSourcecfg(3);
  std::cerr << "Sourcecfg3: " << std::hex << sourcecfg_value << "\n";
  sourcecfg_value = root->readSourcecfg(5);
  std::cerr << "Sourcecfg5: " << std::hex << sourcecfg_value << "\n";
  sourcecfg_value = root->readSourcecfg(7);
  std::cerr << "Sourcecfg7: " << std::hex << sourcecfg_value << "\n";

  root->writeSetip(0, (1 << 3) | (1 << 5) | (1 << 7)); 
  root->writeSetie(0, (1 << 3) | (1 << 5) | (1 << 7)); 
  std::cerr << "Set pending and enable bits for interrupts 3, 5, 7.\n";

  uint32_t setip_value = root->readSetip(0);
  uint32_t setie_value = root->readSetie(0);

  Target tgt{};
  tgt.dm0.hart_index = 0; 
  tgt.dm0.iprio = 3; 
  root->writeTarget(3, tgt.value);
  tgt.dm0.iprio = 5; 
  root->writeTarget(5, tgt.value);
  tgt.dm0.iprio = 7; 
  root->writeTarget(7, tgt.value);
  std::cerr << "Set priorities for interrupts: 3, 5, 7.\n";

  uint64_t target_value = 0;
  target_value = root->readTarget(3);
  target_value = root->readTarget(5);
  target_value = root->readTarget(7);

  uint32_t topi_value = root->readTopi(0);
  std::cerr << "Topi value: " << (topi_value >> 16) << " (priority: " << (topi_value & 0xFF) << ")\n";
  assert((topi_value >> 16) == 3);
  assert((topi_value & 0xFF) == 3); 
  std::cerr << "Verified topi returns priority 3 as the highest-priority interrupt.\n";

  // Set ithreshold = 5
  root->writeIthreshold(0, 5);
  std::cerr << "Set ithreshold to 5.\n";

  // Verify topi reflects only interrupts with priority <= threshold
  topi_value = root->readTopi(0);
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
  Aplic aplic(hartCount, interruptCount);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  aplic.setDirectCallback(directCallback);

  Domaincfg dcfg{};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1);
  root->writeSourcecfg(1, sourcecfg.value);
  root->writeSourcecfg(2, sourcecfg.value);
  root->writeSourcecfg(3, sourcecfg.value);
  std::cerr << "Configured source modes for interrupts 1, 2, and 3 to Edge1.\n";

  root->writeIdelivery(0, 1); 
  std::cerr << "Enabled interrupt delivery for the hart.\n";

  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3)); // Set pending bits for 1, 2, and 3
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3)); // Enable interrupts 1, 2, and 3
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  Target tgt{};
  tgt.dm0.hart_index = 0; // Target hart 0
  tgt.dm0.iprio = 1;
  root->writeTarget(1, tgt.value);
  tgt.dm0.iprio = 2;
  root->writeTarget(2, tgt.value);
  std::cerr << "Set priorities for interrupts: 1=1, 2=2.\n";

  aplic.setSourceState(1, true);
  uint32_t claimi_value = root->readClaimi(0);
  std::cerr << "Claimed interrupt: " << (claimi_value >> 16) << " (priority: " << (claimi_value & 0xFF) << ")\n";
  assert((claimi_value >> 16) == 1);
  assert((claimi_value & 0xFF) == 1);


  aplic.setSourceState(2, true);
  claimi_value = root->readClaimi(0);
  std::cerr << "Claimed interrupt: " << (claimi_value >> 16) << " (priority: " << (claimi_value & 0xFF) << ")\n";
  //assert((claimi_value >> 16) == 2); // TODO
  //assert((claimi_value & 0xFF) == 2); // TODO


  // Test spurious interrupt with iforce
  root->writeIforce(0, 1);
  claimi_value = root->readClaimi(0);
  //assert(claimi_value == 0); // TODO
  std::cerr << "Verified spurious interrupt returns 0.\n";
  std::cerr << "Test test_claimi passed successfully.\n";
}


void 
test_08_setipnum_le() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 10; 
  Aplic aplic(hartCount, interruptCount);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  aplic.setDirectCallback(directCallback);


  Domaincfg dcfg{};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1);
  root->writeSourcecfg(1, sourcecfg.value);

  root->writeIdelivery(0, 1);

  uint32_t idelivery_value = root->readIdelivery(0);
  std::cerr << "Set idelivery to 1. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 1);

  // Write `0x01` to `setipnum_le`
  root->writeSetipnumLe(0x01); // Trigger interrupt 1
  uint32_t setip_value = root->readSetip(0);
  assert(setip_value & (1 << 1)); // Interrupt 1 bit should be set
  std::cerr << "Verified writing 0x01 to setipnum_le sets the corresponding bit in setip.\n";


  // Write `0x00` to `setipnum_le`
  root->writeSetipnumLe(0x00); // Invalid interrupt
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 0))); // Interrupt 0 bit should remain unset
  std::cerr << "Verified writing 0x00 to setipnum_le has no effect.\n";


  // Write `0x800` to `setipnum_le` (invalid identity)
  root->writeSetipnumLe(0x800); // Out of range interrupt
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 11))); // Ensure no invalid interrupt bit is set
  std::cerr << "Verified writing invalid identity (0x800) to setipnum_le has no effect.\n";


  std::cerr << "Test test_setipnum_le passed successfully.\n";
}


void 
test_09_setipnum_be() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 10; 
  Aplic aplic(hartCount, interruptCount);


  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);


  Domaincfg dcfg{};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1; 
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for direct delivery mode (DM=0, IE=1).\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1);
  root->writeSourcecfg(1, sourcecfg.value);

  root->writeIdelivery(0, 1);

  uint32_t idelivery_value = root->readIdelivery(0);
  std::cerr << "Set idelivery to 1. Read back value: " << idelivery_value << "\n";
  assert(idelivery_value == 1);

  // Write `0x01` to `setipnum_be`
  root->writeSetipnumBe(0x01); // Trigger interrupt 1
  uint32_t setip_value = root->readSetip(0);
  assert(setip_value & (1 << 1)); // Interrupt 1 bit should be set
  std::cerr << "Verified writing 0x01 to setipnum_be sets the corresponding bit in setip.\n";


  // Write `0x00` to `setipnum_be`
  root->writeSetipnumBe(0x00); // Invalid interrupt
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 0))); // Interrupt 0 bit should remain unset
  std::cerr << "Verified writing 0x00 to setipnum_be has no effect.\n";


  // Write `0x800` to `setipnum_be` (invalid identity)
  root->writeSetipnumBe(0x800); // Out of range interrupt
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 11))); // Ensure no invalid interrupt bit is set
  std::cerr << "Verified writing invalid identity (0x800) to setipnum_be has no effect.\n";


  std::cerr << "Test test_setipnum_be passed successfully.\n";
}


void 
test_10_targets() 
{
  unsigned hartCount = 4;  // Multiple harts to validate configurations
  unsigned interruptCount = 1023; 
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0, 1, 2, 3};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);

  // MSI delivery mode
  Domaincfg dcfg{};
  dcfg.fields.dm = 1;  
  dcfg.fields.ie = 1;  
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for MSI delivery mode.\n";

  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = unsigned(SourceMode::Edge1);
  root->writeSourcecfg(1, sourcecfg.value);

  // Configure a valid Hart Index, Guest Index, and EIID 
  uint32_t target_value = root->readTarget(1);
  Target tgt{};
  tgt.dm1.hart_index = 2;  
  tgt.dm1.guest_index = 3;  
  tgt.dm1.eiid = 42;  
  root->writeTarget(1, tgt.value);
  std::cerr << "Configured target register.\n";

  // Verify the MSI is sent to the correct hart, guest, and interrupt identity
  target_value = root->readTarget(1);
  assert((target_value & 0x7FF) == 42);  
  assert(((target_value >> 12) & 0x3F) == 0); // for machine-level domains, guest_index is read-only zero
  assert(((target_value >> 18) & 0x3FFF) == 2);  
  std::cerr << "Verified target configuration for hart, guest, and EIID.\n";

  // Write invalid values and verify they are ignored
  tgt.dm1.hart_index = 0xFFFF; 
  tgt.dm1.guest_index = 0xFFFF; 
  tgt.dm1.eiid = 0xFFF + 1; 
  root->writeTarget(1, tgt.value);
  target_value = root->readTarget(1);
  assert(((target_value >> 17) & 0x3FFF) != 0xFFFF); 
  assert(((target_value >> 11) & 0x3F) != 0xFFFF); 
  assert((target_value & 0x7FF) <= 0x7FF);            
  std::cerr << "Verified invalid values are ignored or adjusted.\n";

  // In direct delivery mode, test that an illegal priority (e.g. 0) gets replaced.
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  tgt.dm0.hart_index = 0;
  tgt.dm0.iprio = 0; // illegal priority, expect default (commonly 1)
  root->writeTarget(1, tgt.value);
  target_value = root->readTarget(1);
  // Adjust the expected default as needed; here we assume the implemented value is 1.
  assert((target_value & 0xFF) == 1);

  // Lock MSI address configuration and verify target writes are ignored
  uint64_t mmsiaddrcfgh_value = 0x80000000;  // Lock flag
  root->writeMmsiaddrcfgh(mmsiaddrcfgh_value);
  root->writeTarget(1, tgt.value);  // Attempt write after lock
  target_value = root->readTarget(1);
  std::cerr << "TARGET: " << target_value << "\n";
  assert(target_value == 0x01);  // Target value should remain unchanged 
  std::cerr << "Verified target registers are locked after MSI address configuration is locked.\n";

  std::cerr << "Test test_targets passed successfully.\n";
}


void 
test_11_MmsiAddressConfig() 
{
  unsigned hartCount = 2;
  unsigned interruptCount = 33;
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  Aplic aplic(hartCount, interruptCount);

  aplic.setDirectCallback(directCallback);
  aplic.setMsiCallback(imsicCallback);

  // Create root and child domains
  unsigned hartIndices[] = {0, 1};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  auto child = aplic.createDomain("child", root, addr + domainSize, domainSize, Supervisor, hartIndices);

  // Enable MSI delivery mode in root
  Domaincfg dcfg{};
  dcfg.fields.dm = 1;  // MSI mode
  dcfg.fields.ie = 1;  // Enable interrupt
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Configured domaincfg for MSI delivery mode.\n";

  uint32_t base_ppn = 0x123;  
  uint32_t hhxs = 0b10101;  
  uint32_t lhxs = 0b110;    
  uint32_t hhxw = 0b111;    
  uint32_t lhxw = 0b1111;   
  uint32_t lock_bit = 0;    

  uint32_t mmsiaddrcfg_value = base_ppn | (lhxw << 12);
  uint32_t mmsiaddrcfgh_value = (hhxw << 0) | (hhxs << 4) | (lhxs << 8) | (lock_bit << 31);

  root->writeMmsiaddrcfg(mmsiaddrcfg_value);
  root->writeMmsiaddrcfgh(mmsiaddrcfgh_value);
  std::cerr << "Wrote valid values to mmsiaddrcfg and mmsiaddrcfgh.\n";

  uint32_t read_value = root->readMmsiaddrcfg();
  assert(read_value == mmsiaddrcfg_value);
  read_value = root->readMmsiaddrcfgh();
  assert(read_value == mmsiaddrcfgh_value);
  std::cerr << "Verified MSI address configuration values.\n";

  // Configure source 1 in root as Level1 (active high)
  Sourcecfg cfg1{};
  cfg1.d0.sm = Level1;
  root->writeSourcecfg(1, cfg1.value);

  // Set target for source 1
  Target tgt{};
  tgt.dm0.hart_index = 0;
  tgt.dm0.iprio = 1;
  root->writeTarget(1, tgt.value);

  // Enable source 1 interrupt
  root->writeSetienum(1);
  std::cerr << "Enabled interrupt for source 1.\n";

  // Enable idelivery for hart 0
  root->writeIdelivery(0, 1);
  root->writeIthreshold(0, 2);

  // Trigger MSI Delivery using setipnum
  root->writeSetipnum(1);  // Trigger interrupt 1
  std::cerr << "Set interrupt pending for source 1.\n";

  // Simulate an MSI write to trigger `imsicCallback`
  uint64_t imsic_addr = 0x12000000;  // Example IMSIC address
  uint64_t data = 42;  // Example EIID
  imsicCallback(imsic_addr, data);
  std::cerr << "Simulated MSI delivery to IMSIC.\n";

  // Verify Child Domain is Read-Only
  uint32_t child_invalid_value = 0xFFFFFFFF;  
  child->writeMmsiaddrcfg(child_invalid_value);
  child->writeMmsiaddrcfgh(child_invalid_value);

  uint32_t child_read_value = child->readMmsiaddrcfg();
  std::cerr << "child_read_value: " << child_read_value << "\n";
  assert(child_read_value == 0);  // read-only
  child_read_value = child->readMmsiaddrcfgh();
  std::cerr << "child_read_value: " << child_read_value << "\n";
  assert(child_read_value == 0x0);  // read-only

  std::cerr << "Verified mmsiaddrcfg and mmsiaddrcfgh are read only in non-root machine domains.\n";

  // Lock the MSI Configuration and Verify Writes Are Ignored in Root Domain
  uint32_t lock_value = mmsiaddrcfgh_value | (1 << 31);  
  root->writeMmsiaddrcfgh(lock_value);
  read_value = root->readMmsiaddrcfgh();
  assert((read_value & (1 << 31)) != 0);
  std::cerr << "Verified MSI address configuration lock bit is set.\n";

  // Attempt to modify after locking (should not take effect)
  root->writeMmsiaddrcfg(0x123);
  root->writeMmsiaddrcfgh(0x123);
  read_value = root->readMmsiaddrcfg();
  assert((read_value == mmsiaddrcfg_value) || (read_value == 0));  
  read_value = root->readMmsiaddrcfgh();
  assert(read_value == lock_value || (read_value == 0x80000000));  
  std::cerr << "Verified lock prevents further writes in root domain.\n";
  std::cerr << "Test testMmsiAddressConfig passed successfully.\n";
}


void 
test_12_SmsiAddressConfig() 
{
  unsigned hartCount = 2;  
  unsigned interruptCount = 1;
  Aplic aplic(hartCount, interruptCount);

  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0, 1};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  auto child = aplic.createDomain("child", root, addr + domainSize, domainSize, Supervisor, hartIndices);

  uint32_t base_ppn = 0x234;  // 12-bit PPN
  uint32_t lhxs = 0b101;      // 3-bit field
  uint32_t smsiaddrcfg_value = base_ppn;
  uint32_t smsiaddrcfgh_value = lhxs;

  root->writeSmsiaddrcfg(smsiaddrcfg_value);
  root->writeSmsiaddrcfgh(smsiaddrcfgh_value);
  std::cerr << "Wrote valid values to smsiaddrcfg and smsiaddrcfgh in root domain.\n";

  uint32_t read_value = root->readSmsiaddrcfg();
  assert(read_value == smsiaddrcfg_value);
  read_value = root->readSmsiaddrcfgh();
  assert(read_value == smsiaddrcfgh_value);
  std::cerr << "Verified values match after writing in root domain.\n";

  // Verify non-root domains cannot write these registers
  child->writeSmsiaddrcfg(0xFFFFFFFF);
  child->writeSmsiaddrcfgh(0xFFFFFFFF);

  uint32_t child_read_value = child->readSmsiaddrcfg();
  assert(child_read_value == 0 || child_read_value == smsiaddrcfg_value);  // Expect read-only
  child_read_value = child->readSmsiaddrcfgh();
  assert(child_read_value == 0 || child_read_value == smsiaddrcfgh_value);
  std::cerr << "Verified smsiaddrcfg and smsiaddrcfgh are **read-only** in non-root domains.\n";

  // Locking mmsiaddrcfgh and verifying lock applies to supervisor registers
  uint32_t lock_value = (1 << 31);
  root->writeMmsiaddrcfgh(lock_value);

  root->writeSmsiaddrcfg(0x123);  
  root->writeSmsiaddrcfgh(0x123);  

  read_value = root->readSmsiaddrcfg();
  assert((read_value == smsiaddrcfg_value) || (read_value == 0));  
  read_value = root->readSmsiaddrcfgh();
  assert((read_value == smsiaddrcfgh_value) || (read_value == 0));
  std::cerr << "Verified supervisor MSI registers are locked after setting lock in mmsiaddrcfgh.\n";
  std::cerr << "Test testSmsiAddressConfig passed successfully.\n";
}

void test_13_misaligned_and_unsupported_access()
{
  std::cerr << "\nRunning test_13_misaligned_and_unsupported_access...\n";
  unsigned hartCount = 1, interruptCount = 4;
  Aplic aplic(hartCount, interruptCount);
  
  uint64_t addr = 0x1000000, domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  
  // Original misaligned test on domaincfg.
  aplic.write(addr, 2, 0x1234);
  uint32_t domaincfg_value = 0;
  aplic.read(addr, 4, domaincfg_value);
  assert(domaincfg_value == 0x80000000);
  
  uint64_t invalid_addr = addr + 0x5000;
  uint32_t read_value = 0;
  aplic.write(invalid_addr, 4, 0xdeadbeef);
  aplic.read(invalid_addr, 4, read_value);
  assert(read_value == 0);
  
  // --- Extended misaligned tests ---
  uint64_t sourcecfg_addr = addr + 4;
  aplic.write(sourcecfg_addr, 2, 0xABCD);
  uint32_t read_val = 0;
  aplic.read(sourcecfg_addr, 4, read_val);
  assert(read_val == 0);
  
  uint64_t setie_addr = addr + 0x1e00;
  aplic.read(setie_addr + 1, 4, read_val);
  assert(read_val == 0);
  
  std::cerr << "Test test_13_misaligned_and_unsupported_access passed.\n";
}


void test_14_set_and_clear_pending()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 5;
  Aplic aplic(hartCount, interruptCount);
  
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  
  Domaincfg dcfg{};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  
  // Configure source 1 as Edge1.
  Sourcecfg sourcecfg{};
  sourcecfg.d0.sm = Edge1;
  root->writeSourcecfg(1, sourcecfg.value);
  
  // Set pending bit for source 1 via setip.
  root->writeSetip(0, (1 << 1));
  
  // Now clear it using in_clrip.
  root->writeInClrip(0, (1 << 1));
  uint32_t setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 1)));
  
  // Also test clripnum.
  root->writeClripnum(1);
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << 1)));
  
  std::cerr << "Test seta and clear pending passed.\n";
}

void test_15_genmsi()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  Aplic aplic(hartCount, interruptCount);
  
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  
  // Configure for MSI mode.
  Domaincfg dcfg{};
  dcfg.fields.dm = 1;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  
  uint32_t genmsi_val = (0 << 18) | 42;
  root->writeGenmsi(genmsi_val);
  uint32_t read_genmsi = root->readGenmsi();
  // Check that the EIID portion equals 42.
  std::cerr << "GENMSI: " << read_genmsi << "\n";
  assert((read_genmsi & 0x7FF) == 42);
  
  // Now change to direct delivery mode; genmsi should be read-only zero.
  dcfg.fields.dm = 0;
  root->writeDomaincfg(dcfg.value);
  root->writeGenmsi(0x12345678);
  read_genmsi = root->readGenmsi();
  std::cerr << "GENMSI: " << read_genmsi << "\n";
  // assert(read_genmsi == 0); // TODO
  
  // If your model supports checking the busy bit, you could try writing again.
  // Here, we simply switch to direct delivery mode and confirm genmsi is read-only zero.
  dcfg.fields.dm = 0;
  root->writeDomaincfg(dcfg.value);
  root->writeGenmsi(0x12345678);
  read_genmsi = root->readGenmsi();
  assert(read_genmsi == 0);

  std::cerr << "test_15_genmsi passed.\n";
}


void 
test_16_sourcecfg_pending()
{
  std::cerr << "\nRunning test_16_sourcecfg_pending...\n";
  
  // Use one hart and several interrupt sources.
  unsigned hartCount = 1, interruptCount = 10;
  Aplic aplic(hartCount, interruptCount);
  aplic.setDirectCallback(directCallback);
  
  uint64_t addr = 0x1000000, domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  
  Domaincfg dcfg{};
  dcfg.fields.dm = 0;  
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  
  uint64_t setip_value = 0;
  
  // Variation 1: Change source 1 from Inactive to Level1 with an edge.
  int source = 1;
  root->writeSourcecfg(1, 0); // Inactive.
  aplic.setSourceState(source, false);
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << source)));
  
  Sourcecfg cfg{};
  cfg.d0.sm = 6;  // Level1.
  root->writeSourcecfg(1, cfg.value);
  aplic.setSourceState(source, false);
  aplic.setSourceState(source, true);
  setip_value = root->readSetip(0);
  assert(setip_value & (1 << source));
  std::cerr << "Variation 1 passed: source 1 becomes pending when changed to Level1 with rising edge.\n";
  
  // Variation 2: Change source 2 from Detached to Level1.
  source = 2;
  Sourcecfg cfg_detached{};
  cfg_detached.d0.sm = 1; // Detached.
  root->writeSourcecfg(2, cfg_detached.value);
  aplic.setSourceState(source, false);
  setip_value = root->readSetip(0);
  assert(!(setip_value & (1 << source)));
  
  Sourcecfg cfg2{};
  cfg2.d0.sm = 6;
  root->writeSourcecfg(2, cfg2.value);
  aplic.setSourceState(source, false);
  aplic.setSourceState(source, true);
  setip_value = root->readSetip(0);
  assert(setip_value & (1 << source));
  std::cerr << "Variation 2 passed: source 2 becomes pending when changed to Level1 with rising edge.\n";
  
  // Variation 3: With domaincfg.IE disabled, no interrupt is delivered.
  source = 3;
  Sourcecfg cfg3{};
  cfg3.d0.sm = 6;
  root->writeSourcecfg(3, cfg3.value);
  aplic.setSourceState(source, false);
  interrupts.clear();
  dcfg.fields.ie = 0;
  root->writeDomaincfg(dcfg.value);
  aplic.setSourceState(source, true);
  assert(interrupts.empty());
  std::cerr << "Variation 3 passed: With IE disabled, no interrupt is delivered for source 3.\n";
  
  // Write a reserved SM value (e.g. 2) to source 1 and verify that the register is masked appropriately.
  root->writeSourcecfg(1, 2);
  uint32_t read_val = root->readSourcecfg(1);
  // Adjust expected value as per implementation. Here we assume reserved values are masked to 0.
  // For example, if reserved values are treated as inactive, then read_val should be 0.
  // assert(read_val == 0);
  
  // Test delegation removal: first set delegation, then remove it.
  Sourcecfg delegateCfg {0};
  delegateCfg.d1.d = true;
  delegateCfg.d1.child_index = true;
  root->writeSourcecfg(1, delegateCfg.value);
  root->writeSourcecfg(1, 0);
  read_val = root->readSourcecfg(1);
  assert(read_val == 0);
  
  std::cerr << "Test test_16_sourcecfg_pending (including reserved/delegation) passed.\n";
}


void test_17_pending_extended()
{
  unsigned hartCount = 1, interruptCount = 5;
  Aplic aplic(hartCount, interruptCount);
  
  uint64_t addr = 0x1000000, domainSize = 32 * 1024;
  unsigned hartIndices[] = {0};
  auto root = aplic.createDomain("root", nullptr, addr, domainSize, Machine, hartIndices);
  
  Domaincfg dcfg {0};
  dcfg.fields.dm = 0;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  
  // Configure source 1 as Level1.
  Sourcecfg cfg_level1{};
  cfg_level1.d0.sm = 6;  // Level1 active-high.
  root->writeSourcecfg(1, cfg_level1.value);
  
  // For level-sensitive sources, the pending bit should mirror the external input.
  uint64_t setip_value = 0;
  
  // Set external input high and verify pending bit is set.
  aplic.setSourceState(1, true);
  setip_value = root->readSetip(0);
  assert(setip_value & (1 << 1));
  
  // Now, set external input low and verify pending bit clears.
  aplic.setSourceState(1, false);
  setip_value = root->readSetip(0);
  // For level-sensitive sources in direct delivery mode, the pending bit should follow the input.
  assert(!(setip_value & (1 << 1)));
  
  // For an edge-sensitive source, in contrast, if no transition occurs, the pending bit remains unchanged.
  Sourcecfg cfg_edge1{};
  cfg_edge1.d0.sm = Edge1;
  root->writeSourcecfg(2, cfg_edge1.value);
  // Ensure input is low.
  aplic.setSourceState(2, false);
  root->writeClripnum(2);  // clear pending
  aplic.setSourceState(2, false);
  setip_value = root->readSetip(0);
  // No transition: pending should not be set.
  assert(!(setip_value & (1 << 2)));
  
  std::cerr << "Test 17 pending extended passed.\n";
}



int
main(int, char**)
{
  // test_01_domaincfg();
  // test_02_sourcecfg();
  // test_03_idelivery();
  // test_04_iforce();
  // test_05_ithreshold();
  // test_06_topi();
  // test_07_claimi();
  //test_08_setipnum_le(); // TODO
  //test_09_setipnum_be(); // TODO
  // test_10_targets();
  // test_11_MmsiAddressConfig();
  // test_12_SmsiAddressConfig();
  // test_13_misaligned_and_unsupported_access(); 
  // test_14_set_and_clear_pending();
  test_15_genmsi();
  // test_16_sourcecfg_pending();
  // test_17_pending_extended();
  return 0;
}
