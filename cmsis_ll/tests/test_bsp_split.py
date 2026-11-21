#!/usr/bin/env python3
"""Guard native FX-9A application module ownership.

This prevents regressions where a moved function is accidentally reintroduced
into bsp_board.c or an extracted module is omitted from CMake. It is structural
validation, not runtime/hardware validation.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "cmsis_ll" / "app"
CMAKE = (ROOT / "cmsis_ll" / "CMakeLists.txt").read_text()
ownership = {
    "bsp_board.c": ["board_init", "board_task", "gpio_init", "power_key_init",
                    "power_on", "power_off"],
    "radio_state.c": ["RX_AlignmentInit", "ResetToDefault", "software_init"],
    "frequency_state.c": ["vfo_init", "ContinuousTuning_Enabled",
                          "Frequency_Set", "Frequency_Tune",
                          "VFO_Get", "VFO_Save", "MEM_Get", "MEM_Save"],
    "radio_controls.c": ["MENU_SetUp", "MENU_SetDown", "MENU_Ts", "key_task"],
    "radio_cat.c": ["RadioCAT_Task"],
    "radio_synth.c": ["Get_DDSLO", "Get_DDSBFO"],
    "radio_ui.c": ["board_refresh_manager", "MODE_Refresh",
                   "RadioUI_TuneStarting", "RadioUI_TuneStopped"],
    "radio_rf.c": ["RF_SwrSampleISR", "RF_SwrService", "RF_ToTx", "RF_ToRx",
                   "RF_Tx", "RF_Rx", "Tune_Start", "Tune_Stop", "Tune_Task",
                   "RadioRF_HasSample", "RadioRF_LastSample"],
}
sources = {p.name: p.read_text(encoding="latin1") for p in APP.glob("*.c")}
for filename, names in ownership.items():
    assert filename in sources, f"Missing native module: {filename}"
    assert f"app/{filename}" in CMAKE, f"Module not compiled: {filename}"
    for name in names:
        regex = re.compile(
            r"(?m)^(?:static\s+)?(?:void|bool|uint8_t|uint16_t|uint32_t|int32_t)"
            r"\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{"
        )
        found = [p for p, text in sources.items() if regex.search(text)]
        assert found == [filename], f"{name} ownership mismatch: {found}"
board = sources["bsp_board.c"]
assert len(board.splitlines()) < 500, "Board module is monolithic again"
assert "void key_task(" not in board
assert "void RF_SwrSampleISR(" not in board
assert "void board_refresh_manager(" not in board
print("PASS: eight native modules own radio logic; board handles only bring-up/scheduling")
