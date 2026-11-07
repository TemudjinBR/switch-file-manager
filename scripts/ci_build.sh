#!/usr/bin/env bash
# Executado no contêiner devkitPro pelos workflows de validação.
set -euo pipefail
export DEVKITPRO=/opt/devkitpro DEVKITA64=/opt/devkitpro/devkitA64
export PATH="$PATH:$DEVKITA64/bin:$DEVKITPRO/tools/bin"
make clean
make -j2
python3 scripts/validate_nro.py SwitchFileManager.nro
python3 scripts/validate_nro.py SfmUpdateHelper.nro --title 'Atualizador do Switch File Manager'
python3 tests/run.py --nro SwitchFileManager.nro
SFM_SOURCE_COMMIT="$(git rev-parse HEAD)" python3 scripts/package.py --channel "${SFM_CHANNEL:-experimental}"
cat dist/SwitchFileManager.nro.sha256 dist/SfmUpdateHelper.nro.sha256 dist/SwitchFileManager-v*.zip.sha256
git diff --exit-code
