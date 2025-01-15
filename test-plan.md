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
