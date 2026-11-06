#!/usr/bin/env python3
"""Compatibilidade: as antigas migrações já estão incorporadas ao código."""
import runpy
from pathlib import Path
import sys
scripts = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(scripts))
runpy.run_path(str(scripts / 'prepare.py'), run_name='__main__')
