# APLIC Test Plan

## 4.5.1: domaincfg
- Write `0xfffffffe` to `domaincfg`; expect to read `0x80000104`.
- Write `0xffffffff` to `domaincfg`; expect to read `0x05010080`.

## 4.5.2: sourcecfg
- For a system with `N` interrupt sources, write a non-zero value to a `sourcecfg[i]` where `i` > `N`; expect to read `0x0`.
- Write a non-zero value to a `sourcecfg[i]` in a domain to which source `i` has not been delegated; expect to read `0x0`.
- Delegate a source `i` to a domain and write one of the supported source modes; expect to read that value.
- Write each reserved value for `SM` to a `sourcecfg[i]`; expect to read a legal value.
- After setting `sourcecfg[i]` to a non-zero value in some domain, stop delegating that source to that domain; expect to read `0x0`.
- Make a source `i` not active in a domain, write non-zero values to `ie`, `ip`, and `target`; expect to read `0x0`.
- Write `D`=1 to a `sourcecfg` in a domain with no children; read `D`=0.

## 4.5.5: setip
- Read `setip[0][0]`; expect 0.
- Read bit for a non-implemented source; expect 0.
- Read bit for a pending source; expect 1.
- Write a 1 to a bit corresponding to an active source; expect to read 1 (see 4.7).

## 4.5.6: setipnum
- Write `i` to `setipnum` for an active interrupt source in some domain; expect to read 1 for that bit in `setip` (see 4.7).
- Write `i` to `setipnum` for a non-active source in some domain; expect to read 0 for that bit in `setip`.
- Read `setipnum`; expect to read 0.

## 4.5.7: in\_clrip
- Read `in_clrip` for an interactive source which is not active in a given domain; expect to read 0.
- Read `in_clrip` for an interrupt source which has a rectified input value of 1; expect to read 1.
- Read `in_clrip[0][0]`; expect 0.
- Write a 1 to a bit corresponding to an active and pending source; expect to read 0 in corresponding bit in `setip` (see 4.7).

## 4.5.8: clripnum
- Write `i` to `clripnum` for an active and pending source; expect to read 0 in corresponding bit in `setip` (see 4.7).
- Read `clripnum`; expect 0.

## 4.5.9: setie
- Read `setie[0][0]`; expect 0.
- Read bit for a non-implemented source; expect 0.
- Write `i` for an interrupt source that is active in the domain; expect to read 1.

## 4.5.10: setienum
- Write `i` to `setienum` for an active interrupt source in some domain; expect to read 1 for that bit in `setie`.
- Write `i` to `setienum` for a non-active source in some domain; expect to read 0 for that bit in `setie`.
- Read `setienum`; expect to read 0.

## 4.5.11: clrie
- Write `i` to `clrie` for an active and enabled source; expect to read 0 in corresponding `setie`.
- Read `clrie`; expect to read 0.

## 4.5.12: clrienum
- Write `i` to `clrienum` for an active and enabled source; expect to read 0 in corresponding bit in `setip`.
- Read `clrienum`; expect to read 0.

## 4.5.15: genmsi
- In a domain configured to use MSI mode, write some legal value `x` to `genmsi`; expect to read `x | (1<<12)` until delivered.
- After writing some value `x` to `genmsi`, while busy is still true, write some value `y` to `genmsi`; expect to read `x | (1<<12)`.
- In a domain configured to use direct delivery mode, write to `genmsi`; expect to read 0.
- Write to `genmsi` in a domain with `IE`=0; expect extempore interrupt to be delivered.

## 4.5.16: target
- Write a non-zero value to `target` for an inactive source; expect to read 0.
- In a domain configured to use direct delivery mode, write priority number 0 to `target`; expect to read priority number 1.
- In a domain configured to use direct delivery mode, write priority number `x` to `target` where `x > 2^IPRIOLEN-1`; expect to read `x & (2^IPRIOLEN-1)` in `IPRIO` field.

## 4.6: Reset
- Reset and read `domaincfg`; expect to read `0x80000000` (if system supports little-endian).

## 4.7: Precise effects on interrupt-pending bits
- TODO

## TODO
- MSI address regs
- `setipnum_le`/`setipnum_be`
- `target`
- IDC CSRs
  - idelivery
  - iforce
  - ithreshold
  - topi
  - claimi

## 4.5.3: Machine MSI address configuration (MSI Address Regs: mmsiaddrcfg and mmsiaddrcfgh)
- Write `0x10000000` to `mmsiaddrcfg`; expect MSI writes directed to machine-level interrupt file to be received at the address `0x1000000`.
- Write `0xFFFFFFFF` to `mmsiaddrcfg`; expect to read `0x0` or trigger an error (invalid address).
- Write `0x20000000` to `smsiaddrcfg`; expect MSI writes directed to supervisor-level interrupt file to be received at the address `0x2000000`.
- Write `0xABCDEF00` to both `mmsiaddrcfg` and `smsiaddrcfg`. Send an MSI to `0xABCDEF00` and verify which interrupt file processes the MSI.
- Write `0x0` to `mmsiaddrcfg` and `0x1` to `mmsiaddrcfgh`; expect MSIs to be routed to `0x100000000`.
- Write `0xFFFFFFFF` to both `mmsiaddrcfg` and `mmsiaddrcfgh`; expect to read back `0x0` or generate an error.

## 4.5.13: Set interrupt-pending bit by number, little-endian (setipnum le)
- Write `0x01` to `setipnum_le`. Expect the corresponding bit in `setip` to be set. 
- Write `0x00` to `setipnum_le`. Expect the write to be ignored, and no bit is set in `setip`. 
- Write `0x800` (invalid identity) to `setipnum_le`. Expect no effect.

## 4.5.14 Set interrupt-pending bit by number, big-endian (setipnum be)
- Write `0x01` to `setipnum_be`. Expect the corresponding bit in `setip` to be set. 
- Write `0x00` to `setipnum_be`. Expect the write to be ignored, and no bit is set in `setip`. 
- Write `0x800` (invalid identity) to `setipnum_be`. Expect no effect. 

## 4.5.16 Interrupt targets (target[1]-target[1023])
- Write `0x1` to `target` for a source not active in the domain. Expect to read back `0x0`. (inactive sources)
- Write priority `0x0` to `target` in direct delivery mode. Expect priority `0x1` to be read back. (priority encoding)
- Write priority `0xFF` (assuming `IPRIOLEN = 8`) to `target`. Expect to read back `0xFF`. (priority encoding)
- Write a value exceeding `2^IPRIOLEN - 1` (i.e. `0x200`). Expect to read back `0x200 & (2^IPRIOLEN - 1)`.

## 4.8.1.1 Interrupt delivery enable (idelivery)
- Write `0x1` to `idelivery`. Confirm interrupts are delivered for pending sources. 
- Write `0x0` to `idelivery`. Confirm no interrupts are delivered even if pending. 

## 4.8.1.2 Interrupt force (iforce)
- Write `0x1` to `iforce` for source `i`. Expect `setip[i]` to be set to `1`, regardless of other configurations. 
- Write `0x0` to `iforce` for source `i`. Confirm no change to `setip[i]`. 

## 4.8.1.3 Interrupt enable threshold (ithreshold)
- Write `0x5` to `ithreshold`. Trigger interrupts with priorities from `1` to `10`. Expect only interrupts with priorities `6+` to be delivered. 
- Write `0x0` to `ithreshold`. Confirm all interrupts are delivered. 

## 4.8.1.4 Top interrupt (topi)
- Set multiple pending interrupts with priorities `3`, `5`, and `7`. Read `topi`. Expect the interrupt with priority `3` (highest) to be reported.

## 4.8.1.5 Claim top interrupt (claimi)
- Set interrupt `5` as pending. Read `claimi`. Expect `5` to be reported and removed from `setip`.