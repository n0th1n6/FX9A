#!/usr/bin/env python3
"""Compatibility entry point for transport regressions shared by both branches."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1]/'test_transport.py'), run_name='__main__')
