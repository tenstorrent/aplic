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
- TODO

## 4.5.13: Set interrupt-pending bit by number, little-endian (setipnum le)
- Write `0x01` to `setipnum_le`. Expect the corresponding bit in `setip` to be set.
- Write `0x00` to `setipnum_le`. Expect the write to be ignored, and no bit is set in `setip`.
- Write `0x800` (invalid identity) to `setipnum_le`. Expect no effect.

## 4.5.14 Set interrupt-pending bit by number, big-endian (setipnum be)
- Write `0x01` to `setipnum_be`. Expect the corresponding bit in `setip` to be set.
- Write `0x00` to `setipnum_be`. Expect the write to be ignored, and no bit is set in `setip`.
- Write `0x800` (invalid identity) to `setipnum_be`. Expect no effect. 

## 4.5.16 Interrupt targets (target[1]-target[1023])
- TODO

## 4.8.1.1 Interrupt delivery enable (idelivery)
- Write `0x1` to `idelivery`. Confirm interrupts are delivered for pending sources.
- Write `0x0` to `idelivery`. Confirm no interrupts are delivered even if pending.
- Write `0x1` to `idelivery` for a nonexistent hart. Verify no interrupts are delivered.

## 4.8.1.2 Interrupt force (iforce)
- Write `0x1` to `iforce` for a hart with `idelivery = 1` and `domaincfg.IE = 1`. Verify a spurious interrupt is delivered.
- Write `0x0` to `iforce` and confirm no spurious interrupt is delivered.
- Trigger a spurious interrupt by setting `iforce = 1` and verify `claimi` returns `0`.
- Read `claimi` after a spurious interrupt and confirm `iforce` is cleared to `0`.
- Write `0x1` to `iforce` for a nonexistent hart. Verify no interrupts are delivered.

## 4.8.1.3 Interrupt enable threshold (ithreshold)
- Write `0x0` to `ithreshold`. Verify all pending and enabled interrupts are delivered.
- Write `0x5` to `ithreshold`. Verify only interrupts with priority `6+` are delivered.
- Write `max_priority` (e.g., `2^IPRIOLEN - 1` or `0x200`) to `ithreshold`. Verify no interrupts are delivered.
- Write `0x1` to `ithreshold` and trigger interrupts with priorities `0` and `2`. Verify only priority `0` is delivered.
- Set `domaincfg.IE = 0` and `ithreshold = 0`. Verify no interrupts are delivered.

## 4.8.1.4 Top interrupt (topi)
- Trigger multiple interrupts with priorities `3`, `5`, and `7`. Read `topi`. Verify it returns the interrupt with priority `3` (highest).
- Ensure no interrupts are pending and read `topi`. Verify it returns `0`.
- Set `ithreshold = 5`. Trigger interrupts with priorities `4` and `6`. Verify `topi` only returns the interrupt with priority `4`.
- Verify `topi` always reads `0` for a nonexistent hart.
- Attempt to write to `topi`. Verify the write is ignored.

## 4.8.1.5 Claim top interrupt (claimi)
- Trigger multiple interrupts and read `claimi`. Verify it returns the same value as `topi`.
- Confirm the pending bit for the claimed interrupt is cleared.
- Trigger a spurious interrupt by setting `iforce = 1`. Read `claimi` and verify it returns `0`.
- Ensure no interrupts are pending and read `claimi`. Verify it returns `0`.
- Attempt to write to `claimi`. Verify the write is ignored.
- Set `ithreshold = 5`. Trigger interrupts with priorities `4` and `6`. Read `claimi`. Verify it only claims the interrupt with priority `4`.