# FX-9A Firmware TODO

This file records improvement areas identified during review of the current FX-9A firmware source. The intent is to preserve working radio behavior while improving robustness, hardware compatibility, RF safety, calibration, and maintainability.

## Priority 1 — Robustness and safety

### CAT / UART receive safety
- [ ] Add bounds checking for `UartRxBuf[32]`.
- [ ] If an incoming CAT command exceeds the buffer, discard input until the next `;` and reset the parser cleanly.
- [ ] Add malformed-command handling and optional diagnostic/error counters.

### CAT protocol correctness
- [ ] Initialize all fields in CAT responses before transmission.
- [ ] Fix `IF` response RIT/XIT field so inactive RIT/XIT returns a defined value such as `00000` rather than uninitialized stack data.
- [ ] Correct AF gain scaling:
  - CAT 0–255 -> internal volume 0–60.
  - Internal volume 0–60 -> CAT 0–255.
- [ ] Review the rest of the Kenwood command implementation for incomplete or inconsistent responses.
- [ ] Add host-side CAT regression tests, e.g. `tools/test_cat.py`.

### EEPROM / 24LC16 reliability
- [ ] Make multi-page EEPROM writes fail immediately if any page write fails.
- [ ] Preserve the first error rather than allowing a later successful page write to make the overall operation appear successful.
- [ ] Ensure I2C STOP / bus recovery is performed on NACK/error paths.
- [ ] Consider adding a configuration version and CRC/checksum in unused EEPROM space while retaining compatibility with existing FX-9A EEPROM layouts.
- [ ] Validate saved settings before applying them.

## Priority 2 — RF behavior

### TX / RX sequencing
Current code can assert the physical TX control before the DDS is updated for TX/XIT state.

- [ ] Refactor TX entry so the preferred order is approximately:
  1. Mute RX/audio path.
  2. Select TX filter/mode state.
  3. Program LO/BFO for the intended TX frequency.
  4. Allow the synthesizers to settle if required.
  5. Assert TX control.
  6. Enable microphone or CW keying path.
- [ ] Refactor RX return so RF/keying is removed before restoring RX LO/BFO/filter state and unmuting.
- [ ] Keep OLED/display refresh separate from timing-critical RF switching.
- [ ] Verify carefully with the RF power stages disabled before on-air testing.

### Faster SWR protection
The display-oriented SWR update path currently runs much slower than the ADC sampling capability.

- [ ] Separate fast SWR protection from slow OLED meter updates.
- [ ] Evaluate a protection interval around 5–20 ms instead of roughly 200 ms.
- [ ] Require a small number of consecutive high-SWR readings to reject single-sample spikes.
- [ ] Keep the existing slower smoothing/update rate for the displayed meter.
- [ ] Test shutdown timing into a controlled mismatch/dummy-load setup.

## Priority 3 — Hardware compatibility

### FX-9A filter-switching revisions
`bsp_baud.c` documents both the original filter-switching map and the later 2016-03-09 map, but only one mapping is currently implemented.

- [ ] Convert band/filter GPIO selection to a table-driven implementation.
- [ ] Add at least:
  - `FX9A_BOARD_ORIGINAL`
  - `FX9A_BOARD_2016_03`
- [ ] Start as a compile-time option.
- [ ] Consider a service-menu setting only after both mappings are confirmed on hardware.
- [ ] Document how to identify the applicable board revision.
- [ ] Add regression tests for the expected GPIO/filter code per band.

## Priority 4 — Calibration and measurement

### S-meter, RF power, and SWR calibration
Current meter conversion uses fixed ADC thresholds.

- [ ] Replace hardcoded meter thresholds with table-driven calibration.
- [ ] Add configurable S-meter calibration points, for example S0 / S5 / S9 / S9+20 / S9+40.
- [ ] Add RF power calibration points, for example 1 W / 5 W / 10 W.
- [ ] Add SWR/reflected-power calibration as practical.
- [ ] Store calibration data in unused EEPROM space with versioning/validation.
- [ ] Provide a service/alignment menu rather than exposing raw ADC constants in normal operation.
- [ ] Preserve current defaults so existing radios behave the same without calibration.

## Priority 5 — DDS cleanup

### AD9834 / AD9951 tuning-word math
- [ ] Replace avoidable floating-point DDS calculations with explicit 64-bit integer arithmetic.
- [ ] Add rounding where appropriate.
- [ ] Verify generated tuning words against known frequencies.
- [ ] Add host tests for LO/BFO calculation and tuning-word generation.

### Review dormant phase function
- [ ] Review `ad9834_tx_phase()`.
- [ ] Its input appears to be degrees but the current conversion uses `pi` in the divisor; fix or remove it if it is unused.
- [ ] Do not change active RF behavior until hardware use is confirmed.

## Priority 6 — User-interface improvements

### Main optical VFO encoder
The RES20D-50-201-1 main VFO encoder is a 50 PPR optical quadrature encoder and the existing falling-edge direction scheme is simple and appropriate.

- [ ] Keep the present main encoder decoder unless hardware testing shows missed pulses.
- [ ] Consider optional velocity-based tuning acceleration:
  - slow rotation: fine step,
  - medium rotation: larger step,
  - fast rotation: 1 kHz or greater step.
- [ ] Preserve TS as the explicit coarse-step control.
- [ ] Make acceleration optional or conservative so the radio remains predictable.

### Mechanical encoder handling
- [ ] Continue hardware testing of the newer CH/VOL quadrature decoder.
- [ ] Verify fast-turn response and feel on the actual radio.
- [ ] Keep `FX9A_ENCODER_TRACE` available for diagnosis.

## Priority 7 — Architecture and maintainability

### Reduce `bsp_board.c` responsibilities
The current board file combines radio state, UI state, persistence, DDS control, meter conversion, and TX/RX behavior.

- [ ] Refactor gradually rather than rewriting the firmware.
- [ ] Candidate modules:
  - radio state / mode handling,
  - RF TX/RX sequencing,
  - VFO/frequency handling,
  - persistence/EEPROM,
  - meter conversion/calibration,
  - UI/display refresh,
  - CAT command handling.
- [ ] Preserve EEPROM ABI and known hardware behavior during refactoring.
- [ ] Keep host regression tests around every extracted subsystem.

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

1. CAT/UART buffer safety and CAT correctness.
2. CAT regression tests.
3. EEPROM error handling and recovery.
4. Board/filter revision support.
5. TX/RX sequencing cleanup.
6. Faster SWR protection.
7. Meter calibration.
8. DDS integer-math cleanup.
9. Optional tuning acceleration.
10. Larger architectural cleanup only after the above is stable.
