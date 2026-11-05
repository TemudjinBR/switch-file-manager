#!/usr/bin/env python3
"""Executa testes reais no host; opcionalmente valida o NRO e sua distribuição."""
from pathlib import Path
import argparse
import os
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--nro', type=Path)
args = parser.parse_args()
if args.nro:
    os.environ['SFM_TEST_NRO'] = str(args.nro.resolve())
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'tests'))
suite = unittest.defaultTestLoader.discover(str(ROOT / 'tests'))
result = unittest.TextTestRunner(verbosity=2).run(suite)
sys.exit(not result.wasSuccessful())
