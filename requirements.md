# List of AIA Spec Requirements

1. (§4.2, p. 31) For an interrupt domain below the root, interrupt sources not
   delegated down to that domain appear to the domain as being not implemented.

2. (§4.5, p. 36) Aside from the registers in Table 4.1 and those for IDC
   structures, all other bytes in an interrupt domain's control region are
   reserved and are implemented as read-only zeros.

3. (§4.5.1, p. 37) Only when IE = 1 are pending-and-enabled interrupts actually
   signaled or forwarded to harts.

4. (§4.5.2, p. 37) When source i is not implemented, or appears in this domain
   not to be implemented, sourcecfg[i] is read-only zero.

5. (§4.5.2, p. 37) If source i was not delegated to this domain and is then
   changed (at the parent domain) to become delegated to this domain,
   sourcecfg[i] remains zero until successfully written with a nonzero value.

6. (§4.5.2, p. 38) If an interrupt domain has no children in the domain
   hierarchy, bit D cannot be set to one in any sourcecfg register for that
   domain. For such a leaf domain, attempting to write a sourcecfg register
   with a value that has bit 10 = 1 causes the entire register to be set to
   zero instead.

7. (§4.5.2, p. 38) Whenever interrupt source i is inactive in an interrupt
   domain, the corresponding interrupt-pending and interrupt-enable bits within
   the domain are read-only zeros, and register target[i] is also read-only
   zero.

8. (§4.5.2, p. 38) If source i is changed from inactive to an active mode, the
   interrupt source's pending and enable bits remain zeros, unless set
   automatically for a reason specified later in this section or in Section
   4.7.

9. (§4.5.2, p. 39) When a source is configured as Detached, its wire input is
   ignored; however, the interrupt-pending bit may still be set by a write to a
   setip or setipnum register.

10. (§4.5.2, p. 39) For a source that is inactive or Detached, the rectified
    input value is zero.

11. (§4.5.2, p. 39) A write to a sourcecfg register will not by itself cause a
    pending bit to be cleared except when the source is made inactive.

12. (§4.5.3, p. 39) If no interrupt domain of the APLIC supports MSI delivery
    mode, mmsiaddrcfg and mmsiaddrcfgh are not implemented for any domain.

13. (§4.5.3, p. 39) For domains not at machine level, mmsiaddrcfg and
    mmsiaddrcfgh are never implemented.

14. (§4.5.3, p. 39) When a domain does not implement mmsiaddrcfg and
    mmsiaddrcfgh, the eight bytes at their locations are simply read-only zeros
    like other reserved bytes.

15. (§4.5.3, p. 39) Registers mmsiaddrcfg and mmsiaddrcfgh are potentially
    writable only for the root domain. For all other machine-level domains that
    implement them, they are read-only.

16. (§4.5.3, p. 40) If bit L in mmsiaddrcfgh is set to one, mmsiaddrcfg and
    mmsiaddrcfgh are locked, and writes to the registers are ignored, making
    the registers effectively read-only.

17. (§4.5.3, p. 40) Setting mmsiaddrcfgh.L to one also locks registers
    smsiaddrcfg and smsiaddrcfgh.

18. (§4.5.3, p. 40) For machine-level domains that are not the root domain, if
    these registers are implemented, bit L is always one, and the other fields
    either are read-only copies of mmsiaddrcfg and mmsiaddrcfgh from the root
    domain, or are all zeros.

19. (§4.5.4, p. 41) Registers smsiaddrcfg and smsiaddrcfgh are implemented by a
    domain if the domain implements mmsiaddrcfg and mmsiaddrcfgh and the APLIC
    has at least one supervisor-level interrupt domain. If the registers are
    not implemented, the eight bytes at their locations are simply read-only
    zeros like other reserved bytes.

20. (§4.5.4, p. 41) Like mmsiaddrcfg and mmsiaddrcfgh, registers smsiaddrcfg
    and smsiaddrcfgh are potentially writable only for the root domain. For all
    other machine-level domains that implement them, they are read-only.

21. (§4.5.4, p. 42) If register mmsiaddrcfgh of the domain has bit L set to
    one, then smsiaddrcfg and smsiaddrcfgh are locked as read-only alongside
    mmsiaddrcfg and mmsiaddrcfgh.

22. (§4.5.4, p. 42) For machine-level domains that are not the root domain, if
    smsiaddrcfg and smsiaddrcfgh are implemented and are not read-only zeros,
    then they are read-only copies of the same registers from the root domain.

23. (§4.5.5, p. 42) A read of a setip register returns the pending bits of the
    corresponding interrupt sources. Bit positions in the result value that do
    not correspond to an implemented interrupt source (such as bit 0 of
    setip[0]) are zeros.

24. (§4.5.5, p. 42) On a write to a setip register, for each bit that is one in
    the 32-bit value written, if that bit position corresponds to an active
    interrupt source, the interrupt-pending bit for that source is set to one
    if possible.

25. (§4.5.6, p. 42) If i is an active interrupt source number in the domain,
    writing 32-bit value i to register setipnum causes the pending bit for
    source i to be set to one if possible.

26. (§4.5.6, p. 42) A write to setipnum is ignored if the value written is not
    an active interrupt source number in the domain. A read of setipnum always
    returns zero.

27. (§4.5.7, p. 43) A read of an in clrip register returns the rectified input
    values of the corresponding interrupt sources. Bit positions in the result
    value that do not correspond to an implemented interrupt source (such as
    bit 0 of in clrip[0]) are zeros.

28. (§4.5.7, p. 43) On a write to an in clrip register, for each bit that is
    one in the 32-bit value written, if that bit position corresponds to an
    active interrupt source, the interrupt-pending bit for that source is
    cleared if possible.

29. (§4.5.8, p. 43) If i is an active interrupt source number in the domain,
    writing 32-bit value i to register clripnum causes the pending bit for
    source i to be cleared if possible. A write to clripnum is ignored if the
    value written is not an active interrupt source number in the domain.

30. (§4.5.8, p. 43) A read of clripnum always returns zero.

31. (§4.5.9, p. 43) A read of a setie register returns the enable bits of the
    corresponding interrupt sources. Bit positions in the result value that do
    not correspond to an implemented interrupt source (such as bit 0 of
    setie[0]) are zeros.

32. (§4.5.9, p. 43) On a write to a setie register, for each bit that is one in
    the 32-bit value written, if that bit position corresponds to an active
    interrupt source, the interrupt-enable bit for that source is set to one.

33. (§4.5.10, p. 44) If i is an active interrupt source number in the domain,
    writing 32-bit value i to register setienum causes the enable bit for
    source i to be set to one.  A write to setienum is ignored if the value
    written is not an active interrupt source number in the domain.

34. (§4.5.10, p. 44) A read of setienum always returns zero.

35. (§4.5.11, p. 44) On a write to a clrie register, for each bit that is one
    in the 32-bit value written, the interrupt-enable bit for that source is
    cleared.  A read of a clrie register always returns zero.

36. (§4.5.12, p. 44) If i is an active interrupt source number in the domain,
    writing 32-bit value i to register clrienum causes the enable bit for
    source i to be cleared. A write to clrienum is ignored if the value written
    is not an active interrupt source number in the domain.

37. (§4.5.12, p. 44) A read of clrienum always returns zero.

38. (§4.5.13, p. 44) Register setipnum\_le acts identically to setipnum except
    that byte order is always little-endian, as though field BE (Big-Endian) of
    register domaincfg is zero.

39. (§4.5.14, p. 44) Register setipnum\_be acts identically to setipnum except
    that byte order is always big-endian, as though field BE (Big-Endian) of
    register domaincfg is one.

40. (§4.5.15, p. 45) A write to genmsi causes Busy to become one.

41. (§4.5.15, p. 45) For a machine-level interrupt domain, an extempore MSI is
    sent to the destination hart at machine level, and for a supervisor-level
    interrupt domain, an extempore MSI is sent to the destination hart at
    supervisor level.

42. (§4.5.15, p. 45) Once it has left the APLIC and the APLIC is able to accept
    a new write to genmsi for another extempore MSI, Busy reverts to false.

43. (§4.5.15, p. 45) While Busy is true, writes to genmsi are ignored.

44. (§4.5.15, p. 45) Extempore MSIs are not affected by the IE bit of the
    domain's domaincfg register. An extempore MSI is sent even if domaincfg.IE
    = 0.

45. (§4.5.15, p. 45) When the interrupt domain is configured in direct delivery
    mode
46. (domaincfg.DM = 0), register genmsi is read-only zero.

47. (§4.5.16, p. 46) A write to a target register sets IPRIO equal to bits
    (IPRIOLEN − 1):0 of the 32-bit value written, unless those bits are all
    zeros, in which case the priority number is set to 1 instead.

48. (§4.5.16, p. 46) When interrupt sources have equal priority num- ber, the
    source with the lowest identity number has the highest priority.

49. (§4.5.16, p. 46) For a supervisor-level interrupt domain, a nonzero Guest
    Index is the number of the target hart's guest interrupt file to which MSIs
    will be sent. When Guest Index is zero, MSIs from a supervisor-level domain
    are forwarded to the target hart at supervisor level. For a machine-level
    domain, Guest Index is read-only zero, and MSIs are forwarded to a target
    hart always at machine level.

50. (§4.6, p. 47) Upon reset, writable bits of domaincfg become zero, Busy bit
    in genmsi register becomes zero, and all other state becomes valid and
    consistent.

51. (§4.7, p. 47) If the source mode is Detached:
    - The pending bit is set to one only by a relevant write to a setip or
      setipnum register.
    - The pending bit is cleared when the interrupt is claimed at the APLIC or
      forwarded by MSI, or by a relevant write to an in clrip register or to
      clripnum.

52. (§4.7, p. 47) If the source mode is Edge1 or Edge0:
    - The pending bit is set to one by a low-to-high transition in the
      rectified input value, or by a relevant write to a setip or setipnum
      register.
    - The pending bit is cleared when the interrupt is claimed at the APLIC or
      forwarded by MSI, or by a relevant write to an in clrip register or to
      clripnum.

53. (§4.7, p. 47) If the source mode is Level1 or Level0 and the interrupt
    domain is configured in direct delivery mode (domaincfg.DM = 0):
    - The pending bit is set to one whenever the rectified input value is high.
      The pending bit cannot be set by a write to a setip or setipnum register.
    - The pending bit is cleared whenever the rectified input value is low. The
      pending bit is not cleared by a claim of the interrupt at the APLIC, nor
      can it be cleared by a write to an in clrip register or to clripnum.

54. (§4.7, p. 47) If the source mode is Level1 or Level0 and the interrupt
    domain is configured in MSI delivery mode (domaincfg.DM = 1):
    - The pending bit is set to one by a low-to-high transition in the
      rectified input value. The pending bit may also be set by a relevant
      write to a setip or setipnum register when the rectified input value is
      high, but not when the rectified input value is low.
    - The pending bit is cleared whenever the rectified input value is low,
      when the interrupt is forwarded by MSI, or by a relevant write to an in
      clrip register or to clripnum.

55. (§4.8.1.1, p. 49) Only two values are currently defined for idelivery: 0 =
    interrupt delivery is disabled, 1 = interrupt delivery is enabled.

56. (§4.8.1.2, p. 49) Only values 0 and 1 are allowed for iforce. Setting
    iforce = 1 forces an interrupt to be asserted to the corresponding hart
    whenever both the IE field of domaincfg is one and interrupt delivery is
    enabled to the hart by the idelivery register.

57. (§4.8.1.2, p. 49) When a read of register claimi returns an interrupt
    identity of zero (indicating a spurious inter- rupt), iforce is
    automatically cleared to zero.

58. (§4.8.1.3, p. 50) When ithreshold is a nonzero value P, interrupt sources
    with priority numbers P and higher do not contribute to signaling
    interrupts to the hart, as though those sources were not enabled,
    regardless of the settings of their interrupt-enable bits. When ithreshold
    is zero, all enabled interrupt sources can contribute to signaling
    interrupts to the hart.

59. (§4.8.1.4, p. 50) A read of topi returns zero either if no interrupt that
    is targeted to this hart is both pending and enabled, or if ithreshold is
    not zero and no pending-and-enabled interrupt targeted to this hart has a
    priority number less than the value of ithreshold.

60. (§4.8.1.4, p. 50) Writes to topi are ignored.

61. (§4.8.1.5, p. 50) Register claimi has the same value as topi.

62. (§4.8.1.5, p. 50) When its value is not zero, reading claimi has the
    simultaneous side effect of clearing the pending bit for the reported
    interrupt identity, if possible. 

63. (§4.8.1.5, p. 50) A read from claimi that returns a value of zero has the
    simultaneous side effect of setting the iforce register to zero.

64. (§4.8.1.5, p. 50) Writes to claimi are ignored.

65. (§4.9, p. 51) An MSI is sent for a specific source only when the source's
    corresponding pending and enable bits are both one and the IE field of
    register domaincfg is also one. If and when an MSI is sent, the source's
    interrupt pending bit is cleared.

66. (§4.9.2, p. 53) As soon as a level-sensitive interrupt is forwarded by MSI,
    the APLIC clears the pending bit for the interrupt source and then ignores
    the source until its incoming signal has been de-asserted.
