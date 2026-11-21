"""Locate production radio logic after the board-file split for host harnesses."""
from pathlib import Path


def load_radio_source(bsp: Path) -> str:
    return '\n'.join((bsp / name).read_text(encoding='latin1') for name in (
        'bsp_board.c', 'radio_state.c', 'radio_vfo.c', 'radio_rf.c',
        'radio_ui.c', 'radio_controls.c', 'radio_cat.c'))
