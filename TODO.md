# FX-9A Firmware TODO

Status audited against the source and regression checks on 2026-11-21.

- `[x]` means the stated implementation or software check is complete.
- `[ ]` means implementation, verification, or a decision is still outstanding.
- `master` retains Mbed CE and now includes the application feature backport.
- `cmsis-ll-port` contains the independent CMSIS-Core / STM32 LL firmware.
  Migration checkboxes below refer to that branch, not removal of Mbed from master.

Hardware checks remain separate from software completion. Earlier firmware
startup checks do not validate the latest backport, RF timing, or calibration.

## Priority 0 — CMSIS-Core + STM32 LL migration (`cmsis-ll-port`)

The independent STM32F103 application is implemented on `cmsis-ll-port`, using modern CMSIS-Core/device headers, STM32 LL and GCC/CMake. It builds without Mbed or the bundled legacy STM32 Standard Peripheral Library. Master intentionally retains the Mbed implementation for comparison.

Software migration is complete; full hardware feature parity is still outstanding. Keep the Mbed build and compatibility documentation available while checking each peripheral group on the radio.

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
- [x] Add branch-appropriate vector/IRQ/layout checks: `tools/verify_image.cmake` on master, and `cmsis_ll/tests/verify_boot_image.py`, `verify_full_image.py` plus `platform/device_contracts.c` on the LL branch. Master keeps the TIM5 keyer remap; standalone uses TIM4.
- [ ] Measure physical interrupt/peripheral timing and confirm hardware behavior matches the intended contracts.
- [x] Preserve the existing EEPROM binary layout unless an explicitly versioned migration is introduced.
- [ ] Complete hardware feature-parity checks for the migrated subsystems before retiring the Mbed reference.

### Migration approach used

The migration added subsystem fixes alongside native peripheral calls. The sequence below records that approach; completion is tracked by the checkboxes and feature sections.

Subsystem sequence:

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

- [x] Keep the current Mbed CE image buildable during migration for A/B comparison.
- [x] Compare flash/vector layout and important data structure sizes between old and new builds.
- [x] Extend host tests as logic is extracted from hardware-facing files.
- [ ] Add small hardware smoke-test checkpoints after each peripheral group rather than waiting for the whole migration to finish.
- [ ] Verify in stages with RF power disabled first, then dummy-load RX/TX checks, then controlled on-air testing.
- [x] Retain the Mbed build and compatibility documentation; final removal remains gated on hardware feature parity.

## Development status — Mbed feature backport

The implemented application enhancements from `cmsis-ll-port` have been
backported to the Mbed CE application on `master`. Mbed startup, the bundled
StdPeriph peripheral setup, the 72 MHz clock, and the TIM5 keyer remap remain.
The independent CMSIS/LL migration remains on its own branch.

Checked feature items below mean implemented and covered by software checks;
they do not certify physical RF timing, detector thresholds, calibration, or
board wiring. Run `python3 tools/test_features.py` and build the Mbed image.
See `MBED_PORT.md` for controls, build options, and hardware limitations.

Volume retains the existing save-on-power-button-shutdown policy. Cutting
external power does not save the current setting. No automatic volume-save
change is included.

## Priority 1 — Robustness and safety

### CAT / UART receive safety
- [x] Add bounds checking for `UartRxBuf[32]`.
- [x] If an incoming CAT command exceeds the buffer, discard input until the next `;` and reset the parser cleanly.
- [x] Add malformed-command handling and optional diagnostic/error counters.

### CAT protocol correctness
- [ ] Initialize all fields in CAT responses before transmission.
- [x] Fix `IF` response RIT/XIT field so inactive RIT/XIT returns a defined value such as `00000` rather than uninitialized stack data.
- [x] Correct AF gain scaling:
  - CAT 0–255 -> internal volume 0–60.
  - Internal volume 0–60 -> CAT 0–255.
- [ ] Review the rest of the Kenwood command implementation for incomplete or inconsistent responses.
- [x] Add host-side CAT regression tests for framing, gain/status responses and deferred dispatch; broader interoperability remains open.

### EEPROM / 24LC16 reliability
- [x] Make multi-page EEPROM writes fail immediately if any page write fails.
- [x] Preserve the first error rather than allowing a later successful page write to make the overall operation appear successful.
- [x] Ensure I2C STOP / bus recovery is performed on NACK/error paths.
- [ ] Consider adding a configuration version and CRC/checksum in unused EEPROM space while retaining compatibility with existing FX-9A EEPROM layouts.
- [x] Validate saved settings before applying them.

## Priority 2 — RF behavior

### TX / RX sequencing
TX entry now programs LO/BFO before enabling the physical TX gate. RX return removes RF first. The settling delay and physical transition timing still need measurement.

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
Protection now samples reflected power every 10 ms independently of the slower OLED meter refresh. Ordinary TX requires three consecutive high readings; severe reflection cuts the gate on the first sample.

- [x] Separate fast SWR protection from slow OLED meter updates.
- [x] Evaluate a protection interval around 5–20 ms instead of roughly 200 ms.
- [x] Require a small number of consecutive high-SWR readings to reject single-sample spikes.
- [x] Keep the existing slower smoothing/update rate for the displayed meter.
- [ ] Test shutdown timing into a controlled mismatch/dummy-load setup.

### Automatic-tuner TUNE mode
A dedicated low-power TUNE state drives an external automatic antenna tuner. Long-press V/M in normal VFO mode to start; CAT CA1;/CA0;/CA; provides start/cancel/query. Detector thresholds remain provisional until measured.

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
- [x] Stop TUNE on front-panel cancel/PTT/power-off requests and on completion, severe protection or timeout.
- [ ] Audit cancellation/interlocks for the remaining conflicting CAT/control actions.
- [x] On TUNE exit:
  1. Remove CW carrier/keying.
  2. Return safely to RX.
  3. Restore the user's previous H/L power setting.
  4. Clear tune-specific timers/status.
- [x] Show `TUNE` and keep the SWR bar live while tuning.
- [x] Restore the selected USB/LSB/CW label on exit and hold the final RF-on SWR bar for five seconds as `LAST SWR`, or show `SWR --` if no sample exists.
- [x] Suspend ordinary CW/CQ tasks during the independent TUNE state on master; cover this in the production board-loop regression.
- [x] Select a convenient front-panel long press or key combination for start/cancel without disturbing existing commonly used functions.
- [x] Reuse the existing dormant Kenwood CAT `CA` (CW TUNE) command if practical, e.g. start/query/cancel support after local TUNE behavior is stable.
- [x] Add host-side tests for tune-state transitions, forced LOW power, timeout, SWR success, severe-SWR abort, cancel behavior, and restoration of the previous power setting.
- [ ] Determine final SWR thresholds and timing experimentally on the real radio with a dummy load / controlled mismatch and the RF power stages operated conservatively.

## Priority 3 — Hardware compatibility

### FX-9A filter-switching revisions
Both the original and 2016-03-09 filter maps are implemented in `filter_map.h`. CMake selects the revision; `2016_03` remains the default. Both maps have host tests, while physical revision identification and GPIO verification remain open.

- [x] Convert band/filter GPIO selection to a table-driven implementation.
- [x] Provide compile-time `FX9A_BOARD_REVISION=ORIGINAL` and `2016_03` wiring selections.
- [x] Start as a compile-time option.
- [ ] Consider a service-menu setting only after both mappings are confirmed on hardware.
- [ ] Document how to identify the applicable board revision.
- [x] Add regression tests for the expected GPIO/filter code per band.

## Priority 4 — Calibration and measurement

### S-meter, RF power, and SWR calibration
Optional table-driven calibration is implemented. A validated 30-byte versioned/CRC record at EEPROM address 1104 enables it; absent or invalid calibration retains the original conversion thresholds. Protection continues to use raw ADC thresholds.

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
- [x] Confirm `ad9834_tx_phase()` has no active call sites; correcting the dormant helper does not enable phase control.
- [ ] Verify the active integer DDS frequency calculations on the physical radio.

## Priority 6 — User-interface improvements

### Continuous tuning enable / disable
The long-press BAND menu now provides a persistent CONTINUOUS TUNING ON/OFF option. ON retains cross-band tuning; OFF enforces the selected band limits for front-panel and CAT frequency changes.

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

- [x] Keep the present main encoder decoder unless hardware testing shows missed pulses.
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
The current board file combines radio state, UI state, persistence, DDS control, meter conversion, and TX/RX behavior.

- [x] Extract focused policy helpers and the meter-calibration module while retaining the existing application structure.
- [x] Extract DDS math, filter mapping, tuning policy, TUNE logic, optional VFO acceleration, saved-setting validation, CAT frame validation, and meter calibration into separate helpers/modules.
- [ ] Continue separating the remaining radio state, RF sequencing, VFO persistence, UI refresh and CAT command orchestration from `bsp_board.c`.
- [x] Preserve the legacy EEPROM record sizes/offsets with compile-time assertions and regression tests.
- [ ] Verify physical behavior after the refactoring/backport.
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

1. Validate the latest Mbed backport and standalone changes on the radio: GPIO,
   encoder feel, OLED restoration, audio, EEPROM persistence and CAT operation.
2. Measure DDS settling, TX/RX sequencing and SWR cutoff timing, then establish
   TUNE thresholds with controlled dummy-load/mismatch tests.
3. Identify the fitted filter-board revision and verify both supported maps on
   their respective hardware; document identification before adding a selector.
4. Complete the remaining CAT response audit and broader interoperability and
   recovery tests.
5. Decide how to add version/integrity protection for legacy settings; the
   calibration record already has its own version and CRC. Improve end-to-end
   error reporting from EEPROM save/read wrappers as part of that work.
6. Continue extracting responsibilities from `bsp_board.c` with regression
   coverage. Retire Mbed/legacy compatibility only after hardware feature parity.
