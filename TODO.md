# FX-9A Firmware TODO

Status audited against the implementation, both CLI firmware builds and offline
regressions on 2026-11-21. `[x]` means the implementation/software check is done;
`[ ]` means verification, implementation or a decision remains outstanding.
Master retains Mbed CE; this branch contains the independent CMSIS/LL firmware.
Hardware behavior is not established by a software check.

Software follow-up on 2026-11-23: both branches suspend CW/CQ tasks during
TUNE, keep SWR trip decisions in the RF ISR/service, and retain the calibration
menu with `ERR` on a failed save. Calibration writes are read back before success.
`tools/test_radio_integration.py` covers these paths; hardware checks remain open.

## Priority 0 — Migrate to CMSIS-Core + STM32 LL

**Software port status (development branch):** The standalone complete radio
application now compiles against pinned official STM32F1 CMSIS/LL packages
using native peripheral calls in `cmsis_ll/app/`. Project-owned StdPeriph
compatibility headers, wrappers and driver objects have been eliminated
from this build. The radio's upper-sideband enum is named `RADIO_USB`
while retaining EEPROM value 2. Automated firmware build, host logic
tests and simulated LL register checks are the software gate; **physical
board timing, RF safety and EEPROM behavior still require hardware tests.**
See `cmsis_ll/README.md`.

Make the long-term firmware base independent of Mbed CE and the bundled legacy STM32 Standard Peripheral Library. The target architecture is a lean bare-metal STM32F103 firmware using modern CMSIS-Core/device headers plus STM32 LL, while retaining GCC/CMake and the existing application behavior.

This should **not** be a big-bang rewrite. Migrate and verify one subsystem at a time, keeping the current Mbed CE build as the known-good reference until the replacement path reaches hardware feature parity.

### Target architecture

```text
FX-9A application
  ├── radio state / TX-RX control
  ├── frequency / VFO
  ├── CW keyer and TUNE state
  ├── CAT
  ├── UI / OLED
  ├── meters / protection
  └── persistence
          ↓
project hardware abstraction
          ↓
STM32 LL + CMSIS-Core
          ↓
STM32F103
```

- [x] Keep the existing GCC + CMake build workflow.
- [x] Replace Mbed CE startup/runtime dependencies with project-owned startup, vector table, linker configuration, reset/init path, and system clock setup.
- [x] Use current CMSIS-Core / STM32F1 device headers rather than the old bundled CMSIS copy where practical.
- [x] Replace the 2009 STM32 Standard Peripheral Library usage with STM32 LL or small project-local register-level helpers.
- [x] Avoid STM32 HAL unless a specific peripheral clearly benefits from it; prefer LL/direct access for deterministic radio control.
- [x] Preserve the STM32F103 72 MHz clock behavior and the existing 16 MHz board crystal configuration.
- [x] Preserve interrupt ownership and timing contracts currently checked by `tools/verify_image.cmake`.
- [x] Preserve the existing EEPROM binary layout unless an explicitly versioned migration is introduced.

Preserve radio behavior first; optimize only with regression coverage and the required hardware checks.

### Migration rule: fix each subsystem while porting it

Do not simply translate old StdPeriph/Mbed calls into LL calls. When a subsystem is migrated, also implement the relevant fixes already listed below so old defects are not carried into the new hardware layer.

Suggested subsystem order:

1. **Startup / clock / linker / vector table**
   - establish a fully Mbed-independent bootable image;
   - preserve the current flash base, stack, interrupt vectors, and calibrated timing assumptions.
2. **GPIO / basic board control**
   - power hold, H/L power control, mute, microphone, CW filter/BFO control, TX enable;
   - introduce explicit board-revision definitions where hardware mappings differ.
3. **Timers / scheduler / encoder sampling**
   - preserve the existing 250 us CH/VOL sampling and 5 ms key/PTT scan behavior;
   - keep encoder trace support;
   - provide the timing base needed for fast SWR protection and TUNE mode.
4. **USART + DMA / CAT**
   - migrate the serial hardware;
   - at the same time fix RX buffer overflow handling, malformed commands, CAT response initialization, and AF-gain scaling;
   - add CAT regression tests before considering the subsystem complete.
5. **I2C / 24LC16 EEPROM**
   - migrate the bit-banged or low-level bus implementation;
   - at the same time fix error propagation, STOP/recovery behavior, validation, and future version/CRC support.
6. **DDS control**
   - migrate AD9834 / AD9951 GPIO timing;
   - replace avoidable floating-point tuning-word calculations with explicit integer math;
   - review the dormant AD9834 phase function.
7. **ADC / meters / protection**
   - migrate ADC/DMA handling;
   - separate fast SWR protection from slow display refresh;
   - prepare table-driven meter calibration.
8. **Band/filter switching**
   - move GPIO mapping into board-revision tables;
   - support original and later FX-9A hardware mappings.
9. **OLED / UI**
   - migrate display timing and I/O;
   - add the continuous-tuning ON/OFF option in the long-press BAND settings menu alongside callsign/alignment settings.
10. **Radio state / TX-RX sequencing**
    - move timing-critical RF sequencing out of display-refresh code;
    - prepare/program the correct DDS state before asserting TX.
11. **CW / automatic-tuner TUNE mode**
    - implement the dedicated low-power TUNE state using the new timing/protection infrastructure.
12. **Final Mbed removal**
    - remove Mbed CE and obsolete compatibility glue only after the LL/CMSIS build passes host tests and hardware smoke tests.

### Migration validation

- [x] Keep the Mbed CE image buildable on master for A/B comparison.
- [x] Compare flash/vector layout and important data structure sizes between old and new builds.
- [x] Extend host tests as logic is extracted from hardware-facing files.
- [ ] Add small hardware smoke-test checkpoints after each peripheral group rather than waiting for the whole migration to finish.
- [ ] Verify in stages with RF power disabled first, then dummy-load RX/TX checks, then controlled on-air testing.
- [x] Retain compatibility documentation and the Mbed reference while hardware feature parity remains outstanding.

## Development status — implemented features, hardware verification outstanding

Feature implementations, the application module split, CAT corrections and
versioned settings integrity are present in `cmsis-ll-port` and the Mbed master
backport. The build CLI has produced both full images and passed their branch
image/link checks. Nine build-helper and seven flash-helper offline tests pass;
these checks do not approve physical RF behavior or validate a live flash run.

The TUNE limits (raw reflected-ADC thresholds, hold times, and DDS settling),
SWR trip thresholds, filter wiring maps, calibration points, and TX/RX ordering
need careful controlled measurement with RF disabled first. The new meter
calibration slot does not modify the legacy 44-byte menu record. The
continuous-tuning setting reuses an existing reserved menu byte and defaults ON
when old EEPROM contents have no explicit OFF signature.

Settings now use a fixed save snapshot, verified EEPROM writes, version 1 CRC32
metadata and interrupted-save detection. CAT frame/reply and TX/TUNE interlock
fixes are covered by host regressions. Remaining work is connected-radio/client
validation, physical recovery/timing checks and gradual shared-state cleanup.
Optical VFO acceleration remains optional and OFF by default.

## Priority 1 — Robustness and safety

### CAT / UART receive safety
- [x] Add bounds checking for `UartRxBuf[32]`.
- [x] If an incoming CAT command exceeds the buffer, discard input until the next `;` and reset the parser cleanly.
- [x] Add malformed-command handling and optional diagnostic/error counters.

### CAT protocol correctness
- [x] Initialize all fields in CAT responses before transmission.
- [x] Audit CAT mutation interlocks: cancel conflicting TUNE writes, reject TX/setup mutations and allow queries.
- [x] Fix `IF` response RIT/XIT field so inactive RIT/XIT returns a defined signed Hz value (`+0000` when inactive) rather than uninitialized stack data.
- [x] Correct AF gain scaling:
  - CAT 0–255 -> internal volume 0–60.
  - Internal volume 0–60 -> CAT 0–255.
- [x] Review the rest of the Kenwood command implementation for incomplete or inconsistent responses.
- [x] Add host-side CAT regressions: `tools/test_settings_cat.py` and `tools/test_transport.py` cover framing, replies, addressed VFOs, numeric limits and RF/TUNE interlocks.

- [ ] Verify CAT interoperability with the connected radio and the operator's control software; unsupported features retain fixed compatibility replies.

### EEPROM / 24LC16 reliability
- [x] Make multi-page EEPROM writes fail immediately if any page write fails.
- [x] Preserve the first error rather than allowing a later successful page write to make the overall operation appear successful.
- [x] Ensure I2C STOP / bus recovery is performed on NACK/error paths.
- [x] Add settings version/CRC metadata in unused EEPROM space while preserving legacy record offsets.
- [x] Validate saved settings before applying them.
- [x] Revalidate EEPROM on an explicit save after a boot-time read failure, preserving current RAM edits and retrying shutdown only when stored data is readable and valid. Continued I/O/corruption errors remain protected; host regressions cover legacy and CRC records.
- [x] Verify shutdown writes/readback, detect incomplete saves and propagate errors to the power-button handler; preserve unreadable/corrupt EEPROM at boot.
- [x] Add version 1 settings metadata with CRC32 at EEPROM 1152; preserve legacy offsets and keep calibration separate.
- [ ] Check actual EEPROM shutdown duration, error display and recovery on the radio. See `SETTINGS_AND_CAT.md`.

## Priority 2 — RF behavior

### TX / RX sequencing
TX entry programs LO/BFO before asserting the physical TX gate. RX return removes RF first; actual settling and transition timing still need measurement.

- [x] Refactor TX entry so the preferred order is approximately:
  1. Mute RX/audio path.
  2. Select TX filter/mode state.
  3. Program LO/BFO for the intended TX frequency.
  4. Allow the synthesizers to settle if required.
  5. Assert TX control.
  6. Enable microphone or CW keying path.
- [x] Refactor RX return so RF/keying is removed before restoring RX LO/BFO/filter state and unmuting.
- [x] Keep OLED/display refresh separate from timing-critical RF switching.
- [ ] Verify carefully with the RF power stages disabled before on-air testing.

### Faster SWR protection
Fast protection uses an independent 10 ms sampling path; the display keeps its slower meter updates. Physical shutdown latency still needs measurement.

- [x] Separate fast SWR protection from slow OLED meter updates. Display refresh no longer makes RF shutdown decisions; host tests cover single spikes and sustained trips.
- [x] Evaluate a protection interval around 5–20 ms instead of roughly 200 ms.
- [x] Require a small number of consecutive high-SWR readings to reject single-sample spikes.
- [x] Keep the existing slower smoothing/update rate for the displayed meter.
- [ ] Test shutdown timing into a controlled mismatch/dummy-load setup.

### Automatic-tuner TUNE mode
Add a dedicated low-power TUNE state for driving an external automatic antenna tuner. This should be implemented as its own state machine rather than by reusing normal CW keyer timing.

- [x] Add explicit tune-state functions such as `Tune_Start()`, `Tune_Task()`, and `Tune_Stop()`.
- [x] Suspend ordinary CQ/manual/automatic CW tasks while TUNE is active, with board-loop regression coverage.
- [x] On TUNE start:
  1. Save the current TX power setting.
  2. Force the radio to LOW power before any RF is generated.
  3. Prepare the LO/BFO and TX path for the current displayed RF frequency.
  4. Generate a steady low-power CW carrier using the existing CW TX/carrier path rather than sending a literal Morse dit/dah.
- [x] Do not temporarily change the user's normal `RADIO.mode` just to tune; keep TUNE as a separate operating state so USB/LSB/CW mode and displayed frequency remain intact.
- [x] Keep SWR protection active during TUNE, but use tune-specific persistence:
  - ordinary high SWR may be tolerated briefly because a tuner starts in a mismatched state;
  - extremely high reflected power must still cause an immediate abort;
  - normal TX should retain faster protection.
- [x] Use the existing fast ADC/SWR sampling path rather than slowing the global protection system.
- [x] Auto-complete tuning when SWR remains below a target threshold for a defined settling period (for example a few hundred milliseconds).
- [x] Add a hard maximum tune timeout so carrier generation can never continue indefinitely.
- [x] Stop TUNE immediately on user cancel, PTT, power-off, or another conflicting transmit/control action.
- [x] On TUNE exit:
  1. Remove CW carrier/keying.
  2. Return safely to RX.
  3. Restore the user's previous H/L power setting.
  4. Clear tune-specific timers/status.
- [x] Show a clear OLED indication such as `TUNE` and keep the SWR bar live while tuning.
- [x] Select a convenient front-panel long press or key combination for start/cancel without disturbing existing commonly used functions.
- [x] Reuse the existing dormant Kenwood CAT `CA` (CW TUNE) command if practical, e.g. start/query/cancel support after local TUNE behavior is stable.
- [x] Add host-side tests for tune-state transitions, forced LOW power, timeout, SWR success, severe-SWR abort, cancel behavior, and restoration of the previous power setting.
- [ ] Determine final SWR thresholds and timing experimentally on the real radio with a dummy load / controlled mismatch and the RF power stages operated conservatively.

## Priority 3 — Hardware compatibility

### FX-9A filter-switching revisions
`bsp_baud.c` documents both the original filter-switching map and the later 2016-03-09 map, and both mappings are selectable at build time.

- [x] Convert band/filter GPIO selection to a table-driven implementation.
- [x] Select `FX9A_BOARD_REVISION=ORIGINAL` or `2016_03`; both maps have host tests.
- [x] Start as a compile-time option.
- [ ] Consider a service-menu setting only after both mappings are confirmed on hardware.
- [ ] Document how to identify the applicable board revision.
- [x] Add regression tests for the expected GPIO/filter code per band.

## Priority 4 — Calibration and measurement

### S-meter, RF power, and SWR calibration
Optional table-driven calibration is implemented in a separate versioned/CRC record at EEPROM 1104. Absent or invalid calibration retains the original conversion thresholds; protection still uses raw ADC thresholds.

- [x] Replace hardcoded meter thresholds with table-driven calibration.
- [x] Add configurable S-meter calibration points, for example S0 / S5 / S9 / S9+20 / S9+40.
- [x] Add RF power calibration points, for example 1 W / 5 W / 10 W.
- [x] Add SWR/reflected-power calibration as practical.
- [x] Store calibration data in unused EEPROM space with versioning/validation. Verify write readback and keep the menu/edits with `ERR` on failure so saving can be retried.
- [x] Provide a service/alignment menu rather than exposing raw ADC constants in normal operation.
- [x] Preserve current defaults so existing radios behave the same without calibration.

## Priority 5 — DDS cleanup

### AD9834 / AD9951 tuning-word math
- [x] Replace avoidable floating-point DDS calculations with explicit 64-bit integer arithmetic.
- [x] Add rounding where appropriate.
- [x] Verify generated tuning words against known frequencies.
- [x] Add host tests for LO/BFO calculation and tuning-word generation.

### Review dormant phase function
- [x] Review `ad9834_tx_phase()`.
- [x] Its input appears to be degrees but the current conversion uses `pi` in the divisor; fix or remove it if it is unused.
- [x] Keep the corrected phase helper dormant; it has no active RF call sites.

## Priority 6 — User-interface improvements

### Continuous tuning enable / disable
The current fork allows continuous tuning through the gaps between amateur bands. Add a user setting so this behavior can be enabled or disabled without rebuilding the firmware.

- [x] Add a `CONT TUNE` / `CONTINUOUS TUNING` ON/OFF setting.
- [x] Put this option in the existing settings menu entered by **long-pressing the BAND key** — the same menu that currently contains callsign setup and the RX alignment items.
- [x] Use the existing CH/menu navigation and main tuning control conventions rather than assigning another normal-operation button.
- [x] When **ON**, preserve the current behavior:
  - tune continuously across the full supported range;
  - allow movement through the gaps between amateur bands;
  - keep the appropriate hardware filter selected according to the current continuous-tuning boundary logic;
  - crossing a boundary must not unexpectedly recall another band's saved VFO.
- [x] When **OFF**, restore legacy band-limited tuning behavior:
  - tuning remains inside the currently selected band's configured limits;
  - changing bands is done through the normal band-selection mechanism;
  - saved per-band VFO behavior remains intact.
- [x] Persist the setting in EEPROM while preserving compatibility with existing EEPROM images.
- [x] Choose a safe migration/default rule for old EEPROM records; preferably retain the current fork's continuous-tuning behavior unless the user explicitly disables it.
- [x] Make CAT frequency-setting behavior follow the same policy where practical, or clearly document if CAT is intentionally allowed to tune outside the front-panel band limits.
- [x] Extend `tools/test_frequency.py` to cover both ON and OFF modes, including lower/upper limits, band gaps, VFO recall, RIT/XIT interaction, and CAT frequency commands.

### Main optical VFO encoder

- [x] Acknowledge optical IRQ edges before processing and make TS alignment atomic; injected-edge regressions cover both branches. Fast-turn behavior still requires a physical retest.
The RES20D-50-201-1 main VFO encoder is a 50 PPR optical quadrature encoder and the existing falling-edge direction scheme is simple and appropriate.

- [x] Retain the existing optical encoder decoder; acceleration is optional and defaults OFF. Physical response testing remains open below.
- [x] Consider optional velocity-based tuning acceleration:
  - slow rotation: fine step,
  - medium rotation: larger step,
  - fast rotation: 1 kHz or greater step.
- [x] Preserve TS as the explicit coarse-step control.
- [x] Make acceleration optional or conservative so the radio remains predictable.

### Mechanical encoder handling
- [ ] Continue hardware testing of the newer CH/VOL quadrature decoder.
- [ ] Verify fast-turn response and feel on the actual radio.
- [x] Keep `FX9A_ENCODER_TRACE` available for diagnosis.

## Priority 7 — Architecture and maintainability

### Reduce `bsp_board.c` responsibilities

The Mbed reference split is in `master` commit `987fc770`. The standalone
STM32 LL application now also separates shared state/EEPROM defaults,
VFO/memory policy, LO/BFO math, RF/TUNE/SWR sequencing, OLED/UI, front-panel
controls, and CAT orchestration. `cmsis_ll/app/bsp_board.c` is limited to
GPIO and power bring-up plus the main-loop scheduler. Software build, host
regression and image-layout checks are required on the LL branch; hardware
TX/RX verification remains outstanding.

- [x] Refactor gradually rather than rewriting the firmware.
- [x] Modules extracted:
  - radio state / mode handling,
  - RF TX/RX sequencing,
  - VFO/frequency handling,
  - persistence/EEPROM,
  - meter conversion/calibration,
  - UI/display refresh,
  - CAT command handling.
- [ ] Reduce remaining cross-module access to legacy shared globals in a later behavior-reviewed change.
- [x] Preserve EEPROM record layout and mode values during refactoring (software checks only; hardware behavior remains to be verified).
- [x] Keep host regression tests around every extracted subsystem.

## Existing work to preserve

The current fork already includes several important improvements that should remain protected by regression tests:

- Mbed CE / GCC build support.
- Original EEPROM layout compatibility checks.
- Continuous tuning through amateur-band gaps.
- Updated CH/VOL encoder decoding.
- Receive-only USB/LSB alignment.
- Image/vector/timer/delay verification in the build.
- Host tests for encoder behavior, frequency logic, and RX alignment.

## Remaining work

1. Validate the current Mbed and CMSIS/LL firmware on the radio: GPIO, encoder
   feel, OLED restoration, audio, CAT interoperability and EEPROM persistence.
2. Measure DDS settling, TX/RX sequencing and SWR cutoff timing; establish final
   TUNE thresholds with controlled dummy-load/mismatch tests and verify calibration.
3. Identify the fitted filter-board revision and verify both supported maps on
   their respective hardware before considering a service-menu selector.
4. Exercise EEPROM error/recovery behavior and the complete CLI flash sequence on
   hardware, confirming startup, RX, settings preservation and shutdown timing.
   Settings version/CRC, verified writes and interrupted-save detection are
   implemented and covered by offline tests.
5. Gradually narrow remaining cross-module access to legacy shared radio state
   with regression coverage. Both application module splits are implemented.
   Keep master on Mbed and retain compatibility documentation while hardware
   parity of the independent CMSIS/LL application is assessed.

## Build and flash tooling

- [x] Add a checkout-aware CLI that builds/verifies the full Mbed or CMSIS/LL image and records a SHA256 manifest.
- [x] Chain build-to-flash only after successful build/image checks; keep mandatory matching backups and readback verification.
- [x] Add dry-run, explicit dependency setup, toolchain/build-directory selection, and documented CLI usage.
- [x] Pass nine build-helper and seven offline flash-helper tests, including stale binaries, incompatible caches and verification failures; build and verify both firmware images through the CLI.
- [ ] Exercise the complete CLI flash sequence on the connected radio and confirm physical startup, RX and settings preservation.
