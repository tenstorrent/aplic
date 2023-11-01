APLIC C++ model

# Introduction

This is a C++ model of the RISCV Advanced Platform Interrupt Controller. The
Aplic is a memory mapped device with an address range and a read/write
interface. The system interacts with the Aplic through the Aplic
read/write/SetSourceState methods. The system uses the setSourceState method of
the Aplic to model a change in the interrupt source state of an interrupt. The
Aplic will evaluate the effects of the setSourceState and, if the required
conditions are met, it will deliver/undeliver an interrupt to a hart in the
system. The interrupt delivery details are not part of the Aplic code: The Aplic
relies on a couple of callbacks to perform the delivery. It is up to the code
instantiating the Aplic to define the callback methods. There is one callback
for direct (non MSI) interrupt delivery and one for message based (MSI)
delivery. Here's an overview of the usage mode of the Aplic:

1. Instantiate an Aplic associating it with a memory address, a stride (address
offset between domains), a hart count, a domain count, and an interrupt device
count.

2. Define the direct delivery callback or the MSI delivery callback or both.

3. Define the domains of the Aplic starting with the root domain and associating
each non-root domain with a parent domain.

4. Invoke the Aplic read method whenever there is a memory read operation targeting
an address in the address range of the Aplic.

5. Invoke the Aplic write method whenever there is a memory write operation targeting
an address in the address range of the Aplic.

6. Invoke the setSourceState method whenever there is a change in the state of
an interrupt source associated with the Aplic.


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


# Backdoor Interactions with the Aplic

The system will typically interact with the Aplic through the read, write, and
setSourceState methods. The read/write methods are usually invoked in support of
load/store instructions running on the system harts. The setSourceState is
invoked by asynchronous interrupts coming from IO device models or from an
interrupt generation module in the test bench. In this section we describe
backdoor methods to interact with the Aplic.

# Configuring the Domain Delivery Mode

A domain is configured by writing to its "domaincfg" CSR. Here's an example
of configuring the root domain for IMSIC delivery and enabling its interrupts.

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
csrAddress method maps a CSR number to a memory address.

To configure a domain for direct interrupt deliver, the DM field of the
domaincfg CSR is set to 1:
```
   dcfg.bits_.dm_ = 1;
```

# Configuring Domain Interrupt Sources

An interrupt source is configured in a domain by writing to the address
corresponding to its configuration CSR in that domain.
```
  // Configure source interrupt 1 in root domain as delegated to child 0.
  Sourcecfg cfg1{0};
  cfg1.bits_.d_ = true;
  cfg1.bits_.child_ = 0;
  aplic.write(root->csrAddress(CsrNumber::Sourcecfg1), sizeof(CsrValue), cfg1.value_);
```
