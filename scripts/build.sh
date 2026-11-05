#!/usr/bin/env bash
# Compila e valida em devkitPro nativo ou no ambiente em nuvem já preparado.
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"
python3 tests/run.py
if [[ -n "${DEVKITPRO:-}" && -f "$DEVKITPRO/libnx/switch_rules" ]]; then
    make clean
    make -j"${SFM_BUILD_JOBS:-2}"
elif [[ "$repo_dir" = /workspace/switch-file-manager && -x /workspace/sfm-environment/build.sh ]]; then
    /workspace/sfm-environment/build.sh
    cp /workspace/sfm-environment/artifacts/SwitchFileManager.nro ./SwitchFileManager.nro
else
    echo 'devkitPro indisponível: use o ambiente configurado ou o workflow do GitHub.' >&2
    exit 1
fi
python3 scripts/validate_nro.py SwitchFileManager.nro
python3 tests/run.py --nro SwitchFileManager.nro
python3 scripts/package.py
