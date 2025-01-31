# APLIC C++ Model

## Introduction

This is a C++ model of the RISC-V Advanced Platform-Level Interrupt Controller
(APLIC). For background information on the APLIC, see the [Advanced Interrupt
Architecture Specification](https://github.com/riscv/riscv-aia).

The APLIC is a memory-mapped device with a read/write interface. The system
interacts with the APLIC through the `read`, `write`, and `setSourceState`
methods of the `Aplic` class. The `setSourceState` method is used to model a
change in the state of an interrupt source. The model will evaluate the effects
of the `setSourceState` and will, if the required conditions are met,
deliver/undeliver an interrupt to a hart in the system.

The details of interrupt delivery are outside the scope of the model. Whenever
an interrupt is ready for delivery, the model will invoke one of two
user-provided callbacks to carry out the delivery of the interrupt. Which
callback is invoked depends on the current delivery mode, which may be either
direct delivery or MSI delivery.

Here's an overview of the usage of the `Aplic` class:

1. Instantiate an `Aplic` with a hart count and an interrupt source count.

2. Define the direct delivery callback or the MSI delivery callback or both.

3. Define the domains of the APLIC starting with the root domain and
   associating all other domains with a parent domain.

4. Invoke the `read` and `write` methods whenever there is a load or store
   operation to an address within any of the APLIC's domain control regions.

5. Invoke the `setSourceState` method whenever there is a change in the state
   of an interrupt source associated with the APLIC.

## Compiling

You would need a C++ compiler supporting C++20 or later as well as GNU make.
To compile, issue the command:
```
make
```

## Instantiating an Aplic

The `Aplic` constructor requires the number of controlled harts (1 to 16,384)
and the number of interrupt sources (1 to 1023). If the interrupt source count
is `N`, then the interrupt source IDs will be 1 to `N`.

Example usage:
```
unsigned num_harts = 2;
unsigned num_sources = 33;
TT_APLIC::Aplic aplic(num_harts, num_sources);
```

## Instantiating Domains

A domain is added to an `Aplic` instance using the `createDomain` method.

The `name` parameter allows for assigning a meaningful label to the domain,
which can be useful for logging and debugging. If unique names are used, it
also allows for retrieving domains via the `findDomainByName` method.

The `parent` parameter designates which existing domain is the parent of the
domain being created (thus, parent domains must be created before their
children). When creating the root domain, pass `nullptr`.

The `base` and `size` paramters specify the region of memory occupied by the
domain's control region. This region is subject to the following constraints:

- An alignment of 4 KiB
- A size which is a multiple of 4 KiB
- A minimum size of 16 KiB
- No overlap with control regions of other domains

The `privilege` parameter indicates the privilege level of the domain. The enum
values `Machine` and `Supervisor` are used for machine-level and
supervisor-level domains respectively. As per the spec, the root domain must be
at machine-level, and the parent of any supervisor-level domain must be a
machine-level domain.

The `hart_indices` parameter indicates which harts belong to this domain. These
will constitute the legal values for the hart index field of the target CSR and
the genmsi CSR in the case of MSI delivery mode. As per the spec, a hart may
only belong to one domain at each privilege level, and any harts which belong
to a supervisor-level domain must also belong to its machine-level parent
domain.

Upon success, `createDomain` will return a pointer to the newly created domain.
This pointer can be passed to subsequent calls of `createDomain` via the
`parent` parameter to construct a domain hierarchy.

Note that the order in which child domains are created is significant. The
`sourcecfg` CSRs contain a "child index" field which designates which child
domain to delegate an interrupt source to. The value of this field corresponds
to the order in which child domains are created for a given parent.

If `createDomain` is unable to instantiate a domain, it will throw an exception
with a diagnostic message.

Example usage:
```
uint64_t base = 0x1000000;
uint64_t size = 0x8000;
unsigned hart_indices[] = {0, 1, 2, 3};
auto root = aplic.createDomain("root", nullptr, base, size, TT_APLIC::Machine, hart_indices);
auto child = aplic.createDomain("child", root, base+size, size, TT_APLIC::Supervisor, hart_indices);
```

## Accessing Domain CSRs

CSRs within a domain's control region can be accessed in two ways: via
dedicated per-CSR read and write methods of the `Domain` class and via the
`read` and `write` methods of the `Aplic` class.

Which interface is used will depend on context. If the only thing you know is
that a load or store is being made to some address within an APLIC domain, you
will use the `Aplic`'s read/write interface. But if, for some reason, you want
to access a particular CSR in some domain, it's most convenient to use the
interface provided by the `Domain` class.

### Domain Class CSR Interface

For each CSR in an APLIC domain, the `Domain` class has a method for reading
and a method for writing that CSR. For example, the `domaincfg` CSR has
`readDomaincfg` and `writeDomaincfg`.

Some CSRs are numbered, such as `sourcecfg[i]`. For these CSRs, the first
parameter to the read and write methods is an index.

The read methods all have a return type of `uint32_t`, and the write methods
all have a parameter of type `uint32_t` for the write data. However, some CSRs
have data types for representing the individual fields of the CSR, such as
`Domaincfg`, `Target`, and `Sourcecfg`.

Here is an example of how to use these methods:

```
uint32_t value = root->readDomaincfg();

Domaincfg domaincfg{value};
domaincfg.dm = MSI;
domaincfg.ie = 1;

root->writeDomaincfg(domaincfg.value);
```

These methods enforce various constraints as required by the spec, such as
read-only, WARL, and so forth.

## Aplic Class CSR Interface

As mentioned, in addition to the per-CSR read and write methods in the `Domain`
class, CSRs can also be accessed via the `read` and `write` methods of the
`Aplic` class.

Here is an example usage:

```
uint64_t addr = 0x1000000;
if (aplic.containsAddr(addr)) {
    size_t size = 4;
    uint32_t write_data = 0xdeadbeef;
    aplic.write(addr, size, write_data);

    uint32_t read_data;
    aplic.read(addr, size, read_data);
}
```

These methods check for correct size and alignment and return a boolean
indicating if the access was successful. The read method, therefore, returns
its data by means of an out parameter.

The `containsAddr` method can be used to determine if a given address falls
within one of the control regions for a domain within the APLIC.

## Automatic Forwarding of Interrupts via MSI

By default, for MSI delivery mode, when an interrupt is ready to be forwarded
via MSI, the model will do so automatically. Specifically, it will invoke the
provided MSI callback and clear the interrupt pending bit for the appropriate
source.

However, if this behavior is undesired, there is an option to disable it and to
only forward interrupts via MSI when explicitly instructed to do so. The
`Aplic` class has a member called `autoForwardViaMsi`, which is true by
default. If set to false, an interrupt will only be forwarded via MSI when the
`forwardViaMsi` method is invoked with the interrupt source ID.

This method returns a boolean indicating whether the interrupt of the given ID
was ready to be forwarded. If not, the method does nothing and returns false.

## Reset

The state of the APLIC model can be reset at any time by invoking the `reset`
method. This will leave the domain hierarchy and callback methods unchanged,
but will reset all of the CSRs to their initial values.
