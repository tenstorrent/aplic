#include <iostream>
#include "Aplic.hpp"


using namespace TT_APLIC;

bool directCallback(unsigned hartIx, bool mPrivilege, bool state)
{
  std::cerr << "Delivering interrupt hart=" << hartIx << " privilege="
            << (mPrivilege? "machine" : "supervisor")
            << " interrupt-state=" << (state? "on" : "off") << '\n';
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

int
main(int, char**)
{
  test_01_domaincfg();
  test_02_sourcecfg();

  return 0;
}
