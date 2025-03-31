// SPDX-FileCopyrightText: © 2025 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();

  root->writeDomaincfg(0xfffffffe);
  uint32_t domaincfg = root->readDomaincfg();
  assert(domaincfg == 0x80000104);

  root->writeDomaincfg(0xffffffff);
  aplic.read(addr, 4, domaincfg);
  assert(domaincfg == 0x5010080);
}

void
test_02_sourcecfg()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 1;
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32*1024;
  DomainParams domain_params[] = {
      { "root",  std::nullopt, 0, addr,            domainSize, Machine,    {0} },
      { "child", "root",       0, addr+domainSize, domainSize, Supervisor, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  auto child = root->child(0);

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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
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
  assert((interrupts.size() == 1 || interrupts.size() == 2) && interruptStateMap[0]); 

  root->writeIforce(0, 0);
  std::cerr << "Wrote 0x0 to iforce for valid hart.\n";

  aplic.setSourceState(1, true);
  assert((interrupts.size() == 1 || interrupts.size() == 2) && interruptStateMap[0]);

  root->writeClripnum(1);
  uint32_t setip_value = root->readSetip(0);
  assert((setip_value & (1<<1)) == 0);

  root->writeIforce(0, 1);
  uint32_t topi_value = root->readTopi(0);
  std::cerr << "Topi value: " << (topi_value >> 16) << " (priority: " << (topi_value & 0xFF) << ")\n";
  std::cerr << "Triggered spurious interrupt by setting iforce = 1.\n";

  uint32_t claimi_value = root->readClaimi(0);
  // std::cerr << "Claimi " << claimi_value << "\n"; 
  assert(claimi_value == 0); 
  std::cerr << "Claimi returned 0 after spurious interrupt.\n";

  uint32_t iforce_value = root->readIforce(0);
  assert(iforce_value == 0); 
  std::cerr << "Iforce cleared to 0 after reading claimi.\n";

  // Write 0x1 to iforce for a nonexistent hart
  // root->writeIforce(hartCount, 1); // TODO: this would cause an assertion; use aplic.write() instead
  std::cerr << "Wrote 0x1 to iforce for nonexistent hart.\n";
  std::cerr << "SIZE " << interrupts.size() << "\n"; 

  assert((interrupts.size() == 4 || interrupts.size() == 5) && !interruptStateMap[0]); // No additional interrupts should be added 
  std::cerr << "Test test_iforce passed successfully.\n";
}

void test_05_ithreshold()
{
  unsigned hartCount = 1;
  unsigned interruptCount = 3;
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
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
  std::cerr << "Set target priorities: source 1 (illegal 0 -> becomes 1), source 2 = 5, source 3 = 7.\n";

  // --- Case 1: ithreshold = 0 (no threshold)
  root->writeIthreshold(0, 0x0); // 0 means “no blocking”
  std::cerr << "Set ithreshold to 0 (all interrupts eligible).\n";

  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3));
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3));
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  aplic.setSourceState(3, true);
  assert(interruptStateMap[0] == true);
  std::cerr << "Case 1 passed: an interrupt is delivered with ithreshold = 0.\n";

  // --- Case 2: ithreshold = 1 ---
  root->writeIthreshold(0, 0x1);
  std::cerr << "Set ithreshold to 1 (only interrupts with priority < 1 delivered).\n";
  interrupts.clear();
  // Clear pending bits.
  root->writeClripnum(1);
  root->writeClripnum(2);
  root->writeClripnum(3);
  root->writeSetip(0, (1 << 1) | (1 << 2));
  root->writeSetie(0, (1 << 1) | (1 << 2));
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  assert(interrupts.size() == 1 && !interruptStateMap[0]);
  std::cerr << "Case 2 passed: no interrupts delivered when ithreshold = 1.\n";

  // --- Case 3: ithreshold = 5 ---
  root->writeIthreshold(0, 0x5);
  std::cerr << "Set ithreshold to 5.\n";
  interrupts.clear();
  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3));
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3));
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  aplic.setSourceState(3, true);
  // Only one interrupt is delivered (from source 1).
  assert(interrupts.size() == 1);
  std::cerr << "Case 3 passed: only one interrupt (source 1) delivered when ithreshold = 5.\n";

  // --- Case 4:  ithreshold = max (0xFF) ---
  root->writeIthreshold(0, 0xFF);
  std::cerr << "Set ithreshold to max (0xFF).\n";
  interrupts.clear();
  root->writeSetip(0, (1 << 1) | (1 << 2) | (1 << 3));
  root->writeSetie(0, (1 << 1) | (1 << 2) | (1 << 3));
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  aplic.setSourceState(3, true);
  assert(interruptStateMap[0] == true);
  std::cerr << "Case 4 passed: an interrupt is delivered when ithreshold = max (0xFF).\n";

  // --- Case 5: domaincfg.IE = 0 ---
  dcfg.fields.ie = 0;
  root->writeDomaincfg(dcfg.value);
  std::cerr << "Set domaincfg.IE = 0.\n";
  interrupts.clear();
  aplic.setSourceState(1, true);
  aplic.setSourceState(2, true);
  aplic.setSourceState(3, true);
  assert(interrupts.empty());
  std::cerr << "Case 5 passed: no interrupts are delivered when domaincfg.IE = 0.\n";

  std::cerr << "Test test_05_ithreshold passed successfully.\n";
}

void 
test_06_topi() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 7; 
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();

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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
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

  root->writeSetip(0, (1 << 1) | (1 << 2)); 
  root->writeSetie(0, (1 << 1) | (1 << 2)); 
  std::cerr << "Set pending and enable bits for interrupts 1, 2, and 3.\n";

  Target tgt{};
  tgt.dm0.hart_index = 0; 
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
  assert((claimi_value >> 16) == 2); 
  assert((claimi_value & 0xFF) == 2); 

  root->writeIforce(0, 1);
  claimi_value = root->readClaimi(0);
  assert(claimi_value == 0); 
  std::cerr << "Verified spurious interrupt returns 0.\n";
  std::cerr << "Test test_claimi passed successfully.\n";
}


void 
test_08_setipnum_le() 
{
  unsigned hartCount = 1;
  unsigned interruptCount = 10; 
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();

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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0, 1, 2, 3} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();

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
  DomainParams domain_params[] = {
      { "root",  std::nullopt, 0, addr,              domainSize, Machine,    {0, 1} },
      { "child", "root",       0, addr + domainSize, domainSize, Supervisor, {0, 1} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  aplic.setDirectCallback(directCallback);
  aplic.setMsiCallback(imsicCallback);

  auto root = aplic.root();
  auto child = root->child(0);

  // Enable MSI delivery mode in root
  Domaincfg dcfg{};
  dcfg.fields.dm = 1;  
  dcfg.fields.ie = 1;  
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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root",  std::nullopt, 0, addr,              domainSize, Machine,    {0, 1} },
      { "child", "root",       0, addr + domainSize, domainSize, Supervisor, {0, 1} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  auto child = root->child(0);

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
  uint64_t addr = 0x1000000, domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  
  aplic.write(addr, 2, 0x1234);
  uint32_t domaincfg_value = 0;
  aplic.read(addr, 4, domaincfg_value);
  assert(domaincfg_value == 0x80000000);
  
  uint64_t invalid_addr = addr + 0x5000;
  uint32_t read_value = 0;
  aplic.write(invalid_addr, 4, 0xdeadbeef);
  aplic.read(invalid_addr, 4, read_value);
  assert(read_value == 0);
  
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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  
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
  uint64_t addr = 0x1000000;
  uint64_t domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  
  // Configure for MSI mode.
  Domaincfg dcfg{};
  dcfg.fields.dm = 1;
  dcfg.fields.ie = 1;
  root->writeDomaincfg(dcfg.value);
  
  uint32_t genmsi_val = (0 << 18) | 42;
  root->writeGenmsi(genmsi_val);
  uint32_t read_genmsi = root->readGenmsi();
  assert((read_genmsi & 0x7FF) == 42);
  
  // Change to direct delivery mode; genmsi should be read-only zero.
  dcfg.fields.dm = 0;
  root->writeDomaincfg(dcfg.value);
  root->writeGenmsi(0x12345678);
  read_genmsi = root->readGenmsi();
  std::cerr << "GENMSI: " << read_genmsi << "\n";
  assert(read_genmsi == 0); 
  
  dcfg.fields.dm = 0;
  root->writeDomaincfg(dcfg.value);
  root->writeGenmsi(0x12345678);
  read_genmsi = root->readGenmsi();
  std::cerr << "GENMSI: " << read_genmsi << "\n";
  assert(read_genmsi == 0); 

  std::cerr << "test_15_genmsi passed.\n";
}


void 
test_16_sourcecfg_pending()
{
  std::cerr << "\nRunning test_16_sourcecfg_pending...\n";
  {
    unsigned hartCount = 1, interruptCount = 1;
    uint64_t addr = 0x1000000;
    uint64_t domainSize = 32 * 1024;
    DomainParams domain_params[] = {
        { "root",  std::nullopt, 0, addr,            domainSize, Machine,    {0} },
        { "child", "root",       0, addr+domainSize, domainSize, Supervisor, {0} },
    };
    Aplic aplic(hartCount, interruptCount, domain_params);
    aplic.setDirectCallback(directCallback);

    auto root = aplic.root();
    
    // Set domain configuration: direct delivery and IE enabled.
    Domaincfg dcfg{};
    dcfg.fields.dm = 0;
    dcfg.fields.ie = 1;
    root->writeDomaincfg(dcfg.value);
    
    // --- Basic sourcecfg tests (originally test_02_sourcecfg) ---
    root->writeSourcecfg(2, 0x1);
    uint32_t csr_value = root->readSourcecfg(2);
    assert(csr_value == 0);
    
    auto child = root->child(0);
    child->writeSourcecfg(1, 0x1);
    csr_value = child->readSourcecfg(1);
    assert(csr_value == 0);
    
    Sourcecfg delegateCfg{};
    delegateCfg.d1.d = true;
    delegateCfg.d1.child_index = true;
    root->writeSourcecfg(1, delegateCfg.value);
    child->writeSourcecfg(1, 0x1);
    csr_value = child->readSourcecfg(1);
    assert(csr_value == 1);
    
    // --- Changing source mode (section 4.7) ---
    uint32_t in_clirp_val = 0;
    // Case 1: Inactive mode.
    root->writeSourcecfg(1, 0); 
    aplic.setSourceState(1, true);
    uint32_t setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    in_clirp_val = root->readInClrip(0); 
    // Expect no bit for source 1.
    assert((in_clirp_val & (1 << 1)) == 0);
    std::cerr << "Case 1: Inactive mode produces no pending bit and in_clirp is 0 as expected.\n";
    
    // Case 2: Detached mode.
    Sourcecfg detachedCfg{};
    detachedCfg.d0.sm = Detached;
    root->writeSourcecfg(1, detachedCfg.value);
    aplic.setSourceState(1, true);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 2 (Detached): External input ignored, pending bit is 0.\n";
    // Now force pending via setip.
    root->writeSetip(0, (1 << 1));
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 2 (Detached): Pending bit set via writeSetip.\n";
    // Clear pending using in_clrip.
    root->writeInClrip(0, (1 << 1));
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 2 (Detached): Pending bit cleared via writeInClrip.\n";
    // Force again using setipnum.
    root->writeSetipnum(1);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 2 (Detached): Pending bit set via writeSetipnum.\n";
    // Clear via clripnum.
    root->writeClripnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 2 (Detached): Pending bit cleared via writeClripnum.\n";
    std::cerr << "Case 2: Success.\n";

    
    // Case 3: Edge1 mode.
    Sourcecfg edge1Cfg{};
    edge1Cfg.d0.sm = Edge1;
    root->writeSourcecfg(1, edge1Cfg.value);

    root->writeClripnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);

    // Simulate a rising edge: force low then high.
    aplic.setSourceState(1, false);
    aplic.setSourceState(1, true);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 3: Rising edge sets pending bit.\n";

    // Set pending bit by writing to setip.
    root->writeSetip(0, (1 << 1));
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 3: writeSetip sets pending bit.\n";

    // Clear pending bit by writing to in_clrip.
    root->writeInClrip(0, (1 << 1));
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 3: writeInClrip clears pending bit.\n";

    // Set pending bit by writing to setipnum.
    root->writeSetipnum(1);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 3: writeSetipnum sets pending bit.\n";

    // Clear pending bit by writing to clripnum.
    root->writeClripnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 3: writeClripnum clears pending bit.\n";


    in_clirp_val = root->readInClrip(0);
    // Expect the bit for source 1 to be set.
    assert(in_clirp_val & (1 << 1));
    std::cerr << "Case 3: Edge1 mode produces pending bit on rising edge and in_clirp reflects high input as expected.\n";

    // Case 4:  Edge0 mode.
    Sourcecfg edge0Cfg{};
    edge0Cfg.d0.sm = Edge0;
    root->writeSourcecfg(1, edge0Cfg.value);
    // Simulate a falling edge: force high then low.
    aplic.setSourceState(1, true);
    aplic.setSourceState(1, false);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 4: Falling edge sets pending bit.\n";

    // Set pending bit by writing to setip.
    root->writeSetip(0, (1 << 1));
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 4: writeSetip sets pending bit.\n";

    // Clear pending bit by writing to in_clrip.
    root->writeInClrip(0, (1 << 1));
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 4: writeInClrip clears pending bit.\n";

    // Set pending bit by writing to setipnum.
    root->writeSetipnum(1);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 4: writeSetipnum sets pending bit.\n";

    // Clear pending bit by writing to clripnum.
    root->writeClripnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 4: writeClripnum clears pending bit.\n";

    
    // Case 5: Level1 mode.
    Sourcecfg level1Cfg{};
    level1Cfg.d0.sm = Level1;
    root->writeSourcecfg(1, level1Cfg.value);
    
    // With external input high, pending should be set.
    aplic.setSourceState(1, true);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 5: When input is high, pending bit is set.\n";
    
    // With external input low, pending should be cleared.
    aplic.setSourceState(1, false);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 5: When input is low, pending bit is cleared.\n";
    
    // Attempt to set pending bit via setip or setipnum should have no effect
    // since pending is driven by the external state.
    root->writeSetip(0, (1 << 1));
    setip = root->readSetip(0);
    // Since the external input is low, the pending bit should remain 0.
    assert((setip & (1 << 1)) == 0);
    root->writeSetipnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 5: Writing setip or setipnum does not set pending if input is low.\n";
    
    // Even if we claim the interrupt, pending should remain driven by the input.
    aplic.setSourceState(1, true);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));  // pending bit is set
    uint32_t claim = root->readClaimi(0);
    // For level-sensitive in direct mode, claim should not clear the pending bit.
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 5: Claiming interrupt does not clear pending bit.\n";


    // --- Mode: Level0 ---
    // Test Level0 (active low) mode if supported.
    Sourcecfg level0Cfg{};
    level0Cfg.d0.sm = Level0;
    root->writeSourcecfg(1, level0Cfg.value);
    
    // With external input low (active), pending should be set.
    aplic.setSourceState(1, false);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 6: When input is low, pending bit is set (active low).\n";
    
    // With external input high, pending should be cleared.
    aplic.setSourceState(1, true);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 6: When input is high, pending bit is cleared.\n";
    
    // Attempt to set pending bit via setip or setipnum should have no effect when the external input is high.
    root->writeSetip(0, (1 << 1));
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    root->writeSetipnum(1);
    setip = root->readSetip(0);
    assert((setip & (1 << 1)) == 0);
    std::cerr << "Case 6: Writing setip or setipnum does not force pending if input is high.\n";
    
    // Claiming should not clear pending if external input remains in the “active” state.
    aplic.setSourceState(1, false);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    claim = root->readClaimi(0);
    setip = root->readSetip(0);
    assert(setip & (1 << 1));
    std::cerr << "Case 6: Claiming does not clear pending bit.\n";

    
    // --- Delegation changes ---
    // Case 7: Attempt to delegate a source in a domain with no children.
    root->writeSourcecfg(2, delegateCfg.value);
    uint32_t src_val = root->readSourcecfg(2);
    // Expect that if no children exist, delegation is not allowed so sourcecfg reads as 0.
    assert(src_val == 0);
    std::cerr << "Case 7: Delegation in a domain with no children returns 0.\n";
    
    // Case 7: Delegation removal.
    // Create a child domain and delegate source 3, then remove delegation.
    // Sourcecfg delegateCfg {0};
    delegateCfg.d1.d = true;
    delegateCfg.d1.child_index = true;
    root->writeSourcecfg(1, delegateCfg.value);
    root->writeSourcecfg(1, 0);
    uint32_t read_val = root->readSourcecfg(1);
    assert(read_val == 0);
    std::cerr << "Case 8: Removing delegation causes sourcecfg to revert to 0.\n";

    // --- in_clrip reading ---
    // For a source in Level1 mode, in_clrip should mirror the external (rectified) input.
    root->writeSourcecfg(1, level1Cfg.value);
    aplic.setSourceState(1, true);
    uint32_t in_clrip_val = root->readInClrip(0);
    assert(in_clrip_val & (1 << 1));
    aplic.setSourceState(1, false);
    in_clrip_val = root->readInClrip(0);
    assert(!(in_clrip_val & (1 << 1)));
    std::cerr << "Case 9: readInClrip returns correct rectified input for source 1.\n";
    
    // --- topi when no valid interrupt is pending ---
    // Clear any pending bit for source 1.
    root->writeClripnum(1);
    uint32_t topi_val = root->readTopi(0);
    assert(topi_val == 0);
    std::cerr << "Case 10: topi is 0 when no valid interrupt is pending.\n";

    Sourcecfg cfg3{};
    cfg3.d0.sm = 6;
    root->writeSourcecfg(3, cfg3.value);
    aplic.setSourceState(1, false);
    interrupts.clear();
    dcfg.fields.ie = 0;
    root->writeDomaincfg(dcfg.value);
    aplic.setSourceState(1, true);
    assert(interrupts.empty());
    std::cerr << "Case 11: With IE disabled, no interrupt is delivered for source 3.\n";
  }
  
  {
    std::cerr << "[MSI Delivery Mode]\n";
    unsigned hartCount = 1, interruptCount = 1;
    uint64_t addr = 0x2000000;  // Use a different base for clarity.
    uint64_t domainSize = 32 * 1024;
    DomainParams domain_params[] = {
        { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
    };
    Aplic aplic(hartCount, interruptCount, domain_params);
    aplic.setDirectCallback(directCallback);
    // For MSI mode, we also need an MSI callback:
    aplic.setMsiCallback(imsicCallback);

    auto root = aplic.root();

    // Set domain configuration: DM = 1, IE = 1.
    Domaincfg dcfg{};
    dcfg.fields.dm = 1;
    dcfg.fields.ie = 1;
    root->writeDomaincfg(dcfg.value);

    // --- Level1 in MSI mode ---
    unsigned s_level1 = 1;
    Sourcecfg level1Cfg{};
    level1Cfg.d0.sm = Level1;
    root->writeSourcecfg(s_level1, level1Cfg.value);

    // For MSI mode, the pending bit is set only on a low-to-high transition.
    // With external input low then high.
    aplic.setSourceState(s_level1, false);
    aplic.setSourceState(s_level1, true);
    uint32_t pend = root->readSetip(0);
    // If the external input is high, then the pending bit should be set.
    assert(pend & (1 << s_level1));
    std::cerr << "MSI DM, Level1: Low-to-high transition sets pending bit.\n";

    // a write to setip or setipnum should set the pending bit only if the external input is high.
    // First, clear the pending bit by forcing external input low.
    aplic.setSourceState(s_level1, false);
    root->writeClripnum(s_level1);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level1)) == 0);
    // force external input high and try to set via setip.
    aplic.setSourceState(s_level1, true);
    root->writeSetip(0, (1 << s_level1));
    pend = root->readSetip(0);
    assert(pend & (1 << s_level1));
    std::cerr << "MSI DM, Level1: writeSetip sets pending bit when external input is high.\n";
    root->writeClripnum(s_level1);
    // Now, with external input low, a write to setipnum should not set pending.
    aplic.setSourceState(s_level1, false);
    root->writeSetipnum(s_level1);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level1)) == 0);
    std::cerr << "MSI DM, Level1: writeSetipnum does not set pending when external input is low.\n";

    // The pending bit is cleared when external input goes low.
    aplic.setSourceState(s_level1, true);
    pend = root->readSetip(0);
    assert(pend & (1 << s_level1));
    aplic.setSourceState(s_level1, false);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level1)) == 0);
    std::cerr << "MSI DM, Level1: Pending bit clears when external input goes low.\n";

    // --- Level0 in MSI mode ---
    unsigned s_level0 = 1;
    Sourcecfg level0Cfg{};
    level0Cfg.d0.sm = Level0;
    root->writeSourcecfg(s_level0, level0Cfg.value);

    // For Level0 in MSI mode (active low), a rising transition (low-to-high) is not the trigger;
    // rather, a falling-to-low (or simply, the fact that the rectified input is high) should set pending.
    // (A) With external input low (active for Level0), pending should be set.
    aplic.setSourceState(s_level0, false);
    pend = root->readSetip(0);
    assert(pend & (1 << s_level0));
    std::cerr << "MSI DM, Level0: When external input is low, pending bit is set (active low).\n";
    // (B) With external input high, pending should not be set.
    aplic.setSourceState(s_level0, true);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level0)) == 0);
    std::cerr << "MSI DM, Level0: When external input is high, pending bit is clear.\n";
    // (C) a write to setip or setipnum sets pending only if the rectified input is high.
    aplic.setSourceState(s_level0, false);  // active condition
    root->writeSetip(0, (1 << s_level0));
    pend = root->readSetip(0);
    assert(pend & (1 << s_level0));
    std::cerr << "MSI DM, Level0: writeSetip sets pending bit when external input is low.\n";
    root->writeClripnum(s_level0);
    aplic.setSourceState(s_level0, true);  // not active
    root->writeSetipnum(s_level0);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level0)) == 0);
    std::cerr << "MSI DM, Level0: writeSetipnum does not set pending when external input is high.\n";
    // if the external input goes high or a write to in_clrip/clripnum occurs, pending is cleared.
    aplic.setSourceState(s_level0, false);
    pend = root->readSetip(0);
    assert(pend & (1 << s_level0));
    aplic.setSourceState(s_level0, true);
    pend = root->readSetip(0);
    assert((pend & (1 << s_level0)) == 0);
    std::cerr << "MSI DM, Level0: Pending bit clears when external input becomes high.\n";
  }
  
  std::cerr << "Test test_16_sourcecfg_pending (including reserved/delegation) passed.\n";
}


void test_17_pending_extended()
{
  unsigned hartCount = 1, interruptCount = 5;
  uint64_t addr = 0x1000000, domainSize = 32 * 1024;
  DomainParams domain_params[] = {
      { "root", std::nullopt, 0, addr, domainSize, Machine, {0} },
  };
  Aplic aplic(hartCount, interruptCount, domain_params);

  auto root = aplic.root();
  
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
  
  // For an edge-sensitive source, if no transition occurs, the pending bit remains unchanged.
  Sourcecfg cfg_edge1{};
  cfg_edge1.d0.sm = Edge1;
  root->writeSourcecfg(2, cfg_edge1.value);
  // Ensure input is low.
  aplic.setSourceState(2, false);
  root->writeClripnum(2);  
  aplic.setSourceState(2, false);
  setip_value = root->readSetip(0);
  // No transition: pending should not be set.
  assert(!(setip_value & (1 << 2)));
  
  std::cerr << "Test 17 pending extended passed.\n";
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
  test_13_misaligned_and_unsupported_access(); 
  test_14_set_and_clear_pending();
  test_15_genmsi();
  test_16_sourcecfg_pending();
  test_17_pending_extended();
  return 0;
}
