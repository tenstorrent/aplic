APLIC C++ model

# Introduction

This is a C++ model of the RISCV Advanced Platform Interrupt Controller.

# Compiling
You would need a C++ compiler supporting c++20 or later as well as GNU make.
To compile, issue the command:
```
make
```

# Instantiating an Aplic

The constructor requires the memory address of the Aplic device, the number of
controlled harts, the stride between memory regions associated with the domains
of the Aplic, the domain count, and the interrupt source count. If the interrupt
source count is n, then the interrupt source ids will be 1 to n-1.
Here's a example:
```
  unsigned hartCount = 2;
  unsigned interruptCount = 33;
  unsigned domainCount = 2;

  uint64_t addr = 0x1000000;
  uint64_t stride = 32*1024;
  Aplic aplic(addr, stride, hartCount, domainCount, interruptCount);
```

# Instantiating a Domain

A domain is instnatiated from an Aplic object using the createDomain method. The
root domain must be created first. Child domain must be created after their
parents are created. A domain is associated with a memory address, a parent
domain, and a privilege mode. The parent domain of the root domain is the null
pointer. The address must have a value that matches the pattern "addr + n*stride"
where addr is the address of the Aplic, stride is its stride, and n is an integer
between 0 and the one minus the associated domain count. The root domain
must be at machine privilege.
```
  bool isMachine = true;
  auto root = aplic.createDomain(nullptr, addr, isMachine);
```

## Instantiating a Child domain

Here we must pass a parent domain to the createDomain method. Here's sample
code instantiating a supervisor privilege domain at domain slot 1:
```
  auto child = aplic.createDomain(root, addr + 1*stride, isMachine);
```
The createDomain method will return a nullptr if the given address is not
valid or if it is already occupied by another domain.

# Configuring a Domain for direct delivery.

A domain is configured by writing to its "domaincfg" CSR. Here's an example
of configuring the root domain for direct delivery and enabling its interrupts.

```
  // Read the domain config CSR.
  uint64_t value = 0;
  root->read(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), value);

  // Set the direct delivery mode. Set the interrupt enable bit.
  Domaincfg dcfg{CsrValue(value)};
  dcfg.bits_.dm_ = 0;
  dcfg.bits_.ie_ = 1;

  // Write the value back.
  root->write(root->csrAddress(CsrNumber::Domaincfg), sizeof(CsrValue), dcfg.value_);
```

The write method requires an address (of a memory mapped register), the
csrAddress method maps a CSR number ot a memory address.

