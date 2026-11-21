# FX-9A Firmware TODO

This file records improvement areas identified during review of the current FX-9A firmware source. The intent is to preserve working radio behavior while improving robustness, hardware compatibility, RF safety, calibration, and maintainability.

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
- [ ] Preserve radio behavior first; optimize/clean up only after each migrated subsystem is covered by tests or hardware checks.

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
- [ ] Do not remove the current compatibility documentation until the CMSIS/LL path is proven on the actual radio.

## Development status — software complete, hardware verification outstanding

Feature implementations and pure-logic tests are included in the independent
`cmsis-ll-port` firmware. The combined GitHub Actions run 44717229573
passed the build, vector, existing radio regressions, and new feature tests.
Checks below marked complete mean **implemented in software**, not physically
measured or approved for RF transmission.

The TUNE limits (raw reflected-ADC thresholds, hold times, and DDS settling),
SWR trip thresholds, filter wiring maps, calibration points, and TX/RX ordering
need careful controlled measurement with RF disabled first. The new meter
calibration slot does not modify the legacy 44-byte menu record. The
continuous-tuning setting reuses an existing reserved menu byte and defaults ON
when old EEPROM contents have no explicit OFF signature.

Remaining software improvements include exhaustive CAT compatibility and
recovery tests, stronger EEPROM transaction error propagation and integrity
checks for the original settings format, and gradual source organization.
Optical VFO acceleration is implemented but disabled by default until verified
on the actual encoder. Those items
remain unchecked below.

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
- [x] Consider adding a configuration version and CRC/checksum in unused EEPROM space while retaining compatibility with existing FX-9A EEPROM layouts.
- [x] Validate saved settings before applying them.
- [x] Verify shutdown writes/readback, detect incomplete saves and propagate errors to the power-button handler; preserve unreadable/corrupt EEPROM at boot.
- [x] Add version 1 settings metadata with CRC32 at EEPROM 1152; preserve legacy offsets and keep calibration separate.
- [ ] Check actual EEPROM shutdown duration, error display and recovery on the radio. See `SETTINGS_AND_CAT.md`.

## Priority 2 — RF behavior

### TX / RX sequencing
Current code can assert the physical TX control before the DDS is updated for TX/XIT state.

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
The display-oriented SWR update path currently runs much slower than the ADC sampling capability.

- [x] Separate fast SWR protection from slow OLED meter updates.
- [x] Evaluate a protection interval around 5–20 ms instead of roughly 200 ms.
- [x] Require a small number of consecutive high-SWR readings to reject single-sample spikes.
- [x] Keep the existing slower smoothing/update rate for the displayed meter.
- [ ] Test shutdown timing into a controlled mismatch/dummy-load setup.

### Automatic-tuner TUNE mode
Add a dedicated low-power TUNE state for driving an external automatic antenna tuner. This should be implemented as its own state machine rather than by reusing normal CW keyer timing.

- [x] Add explicit tune-state functions such as `Tune_Start()`, `Tune_Task()`, and `Tune_Stop()`.
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
Current meter conversion uses fixed ADC thresholds.

- [x] Replace hardcoded meter thresholds with table-driven calibration.
- [x] Add configurable S-meter calibration points, for example S0 / S5 / S9 / S9+20 / S9+40.
- [x] Add RF power calibration points, for example 1 W / 5 W / 10 W.
- [x] Add SWR/reflected-power calibration as practical.
- [x] Store calibration data in unused EEPROM space with versioning/validation.
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
- [ ] Do not change active RF behavior until hardware use is confirmed.

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
The RES20D-50-201-1 main VFO encoder is a 50 PPR optical quadrature encoder and the existing falling-edge direction scheme is simple and appropriate.

- [ ] Keep the present main encoder decoder unless hardware testing shows missed pulses.
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

## Suggested implementation order

The CMSIS-Core + STM32 LL migration is the umbrella workstream. Apply the other TODO fixes when their subsystem is migrated rather than postponing them until after the port.

1. Establish Mbed-independent startup / clock / linker / vector support.
2. Migrate GPIO and basic board control.
3. Migrate timers and input scanning while preserving current encoder/key timing.
4. Migrate USART/DMA and complete the CAT/UART safety + correctness fixes and tests.
5. Migrate I2C/EEPROM and complete error handling, validation, and recovery improvements.
6. Migrate DDS control and complete integer-math cleanup.
7. Migrate ADC/protection and implement faster SWR protection.
8. Migrate band/filter control and add hardware-revision support.
9. Migrate OLED/UI and add the continuous-tuning enable/disable setting.
10. Refactor TX/RX sequencing around the new hardware layer.
11. Implement the automatic-tuner TUNE state.
12. Add meter calibration and optional VFO acceleration.
13. Remove Mbed CE / legacy StdPeriph only after the CMSIS/LL path is proven on hardware.
14. Reduce remaining direct access to shared radio globals after the module split; validate physical behavior before further RF changes.
