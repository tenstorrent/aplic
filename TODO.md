# TODO

- Add API documentation
- Require that `size` of domain's control region be large enough to hold an IDC
  for each potential hart index number.
- Consider making various methods of `Domain` class public for convenience,
  such as `sourceIsImplemented()`, `sourceIsActive()`, `pending()`,
  `enabled()`, `parent()`, etc.
- Consider making it possible to set callbacks on a per-domain basis. This
  could be useful, for example, if one wanted to print diagnostic information
  for a specific domain.
- In several places, the AIA spec allows for implementations to choose among
  multiple options. The model should allow for configuring such options. Here
  is a (probably incomplete) list:
  - Starting at offset 0x4000, an interrupt domain's control region may
    optionally have an array of interrupt delivery control (IDC) structures.
  - For each IDC structure in the array that does not correspond to a valid
    hart index number in the domain, the IDC structure's registers may (or may
    not) be all read-only zeros.
  - A given APLIC implementation may support either or both delivery modes for
    each interrupt domain.
  - For RISC-V systems that support only little-endian, BE may be read-only
    zero, and for those that support only big-endian, BE may be read-only one.
  - Inactive (zero) is always supported for field SM. Implementations are free
    to choose, independently for each interrupt source, what other values are
    supported for SM.
  - `mmsiaddrcfg` and `mmsiaddrcfgh` may or may not be implemented for other
    machine-level domains.
  - When `mmsiaddrcfg` and `mmsiaddrcfgh` are writable (root domain only), all
    fields other than L are WARL. An implementation is free to choose what
    values are supported.
  - When L = 1, the other fields in `mmsiaddrcfg` and `mmsiaddrcfgh` may
    optionally all read as zeros.
  - For the root domain, L is initialized at system reset to either zero or
    one.

## Questions:

- Should CSRs in the IDC be writable only in direct delivery mode?
- Allow harts to be in multiple domains at the same privilege level when in MSI
  delivery mode? Maybe emit a warning or error in such cases if direct delivery
  mode is supported.
- Should `reset()` leave the source states unchanged?
