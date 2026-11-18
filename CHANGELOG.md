# Changelog

## 2026-11-18

### Changed

- Corrected startup power-hold sequencing to match the original radio firmware, allowing the radio to start from its normal power supply.
- Enabled continuous tuning from 1.79900 to 29.70099 MHz, including the gaps between amateur bands. Band selection changes at each band's lower boundary, and tuning stops at the overall frequency limits.
- Applied continuous tuning to the main tuning encoder and CAT frequency commands. VFO and memory recall now derive the band from the stored frequency.
- Improved CH and VOL encoder decoding with 250 µs sampling and one action per complete quadrature cycle. Impossible two-contact jumps retain the last valid state so contact chatter does not erase valid click progress. Button, PTT, and paddle scanning remains at 5 ms.

### Added

- Separate receive-only USB and LSB alignment settings on a new RX ALIGNMENT menu page after the callsign entries. Each offset ranges from -1000 to +1000 Hz, with 10 Hz steps or 100 Hz steps when TS is enabled.
- Receive LO and BFO adjustments that move together to preserve the displayed RF tuning frequency. Both offsets default to zero; transmit and CW frequencies are unchanged.
- Compatible storage of the receive alignment settings in the existing menu EEPROM record, using formerly reserved bytes without moving other settings. Settings are saved during normal power-off.
- Optional encoder contact recording for ST-Link diagnosis, plus regression checks for tuning, encoder behavior, recorded contact sequences, receive alignment, menu navigation, and EEPROM compatibility.

### Validation

- Built the firmware and passed the linked-image compatibility and host regression checks.
- Flashed the final build through ST-Link and verified it against a full image readback. Confirmed that the CPU was running without reported faults and that both receive alignment offsets initialized to zero.
- Listening tests are still needed to choose alignment offsets for the individual radio; no measured sensitivity or audio-quality improvement is claimed.

## 2017-03-08 — AK4T

- Increased the delay values to match the 72 MHz MCU clock and address intermittent instability.

## 2017-02-24 — BG6RDF

- Changed the default narrow CW BFO offset so wide and narrow CW modes both receive using the upper sideband by default.
- Thanks to AK4T for pointing out that switching between narrow and wide CW reception selected different sidebands.

## 2017-02-18 — BG6RDF

- Changed the LO and BFO frequency settings in wide CW mode to address spurious reception.

## 2017-02-14 — BG6RDF

- Fixed a serious callsign-setting error caused by menu positions shifting when the narrow CW LO offset setting was added in the previous update.
- Added a factory reset to recover from inconsistent EEPROM settings after a firmware upgrade.

### Factory reset

Hold POWER, A/B, and TS/RIT together during startup until the display lights up.

## 2017-02-05 — BG6RDF

- Increased the MCU clock to 72 MHz.
- Reduced key and button polling to 5 ms to improve keying response and make CW dot/dash timing more consistent.
- Added an adjustable LO offset for narrow CW reception to compensate for differences in the narrow CW filter's center frequency.
- Added debounce handling for the small rotary encoders.
- Fixed incorrect transmit frequency when switching from narrow CW reception to transmission.
- Added smoothing for the receive and transmit signal indications.
- Changed memory editing: short-press M/V to enter memory mode, then long-press M/V to edit. Use CH to select the memory channel and PO/WPM to select its band.
- Made the receive frequency follow changes while editing a memory channel, instead of updating only after leaving the editor.

## 2017-01-26 — BG6RDF

- Fixed incorrect CW keying or subsequent automatic calls after interrupting an automatic call.
- Increased the callsign length from eight to ten characters.
- Fixed a hang after an automatic call when all callsign character positions were filled.
- Added XIT mode. Short-pressing BAND now selects XIT instead of cycling bands, since band selection is already available through the BAND encoder.
- Fixed incorrect receive frequency after returning to reception in RIT mode.
- Changed startup from a brief POWER press to holding POWER.

### Historical upgrade instructions

1. Long-press BAND to open the settings menu and record the existing settings before flashing.
2. Follow the original flashing instructions in `刷机过程.docx`.
3. After flashing, check the callsign editor. If the final two characters are invalid, correct them with the encoder.
4. Long-press POWER to shut down and complete the upgrade.

### Historical recovery instructions

The following procedure clears stored settings:

1. Flash the original `Clear24Lc16` EEPROM-clearing HEX image and power on the radio to erase its stored configuration.
2. Flash the controller firmware again.
3. Reconfigure the radio through its settings menu.
