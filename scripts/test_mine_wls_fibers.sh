#!/usr/bin/env bash
# Reproducible, standalone Geant4 benchmark. Does not launch production GEMC.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# Current spectral profiles use the spectral benchmark; preserve the old
# implementation below for historical scalar profiles.
if rg -q '^fiberLossModel=kuraray_spectral' "$repo/experiments/mine/geant4_optics.conf"; then
    exec bash "$repo/scripts/test_mine_wls_spectral_loss.sh" "$@"
fi
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
photons=6000
output=
while (($#)); do
    case "$1" in
        -h|--help) cat <<'HELP'
Usage: scripts/test_mine_wls_fibers.sh [--photons N] [--output-dir NEW_DIR]
Compile the native Geant4 spectral and straight-fiber tests in the running GEMC
container. Test Y-11, YS-6, YS-4 and a 600-nm analytic absorption control.
Default 6000 photons/profile, seed 20261120; no photon time cut.
The long-component fit uses nested plane counts with a multinomial likelihood.
Outputs CSV, logs, provenance hashes, fit JSON and PNG. Python needs numpy,
matplotlib; Docker needs the existing /work/clas12Tags_mine mount.
This checks an approximate common loss model, not the full Kuraray protocol.
HELP
        exit 0;;
        --photons|--output-dir) [[ $# -ge 2 ]] || exit 2
            if [[ $1 == --photons ]]; then photons=$2;else output=$2;fi;shift 2;;
        *) printf 'Unknown option: %s\n' "$1" >&2;exit 2;;
    esac
done
[[ $photons =~ ^[0-9]+$ ]] && ((photons>=100 && photons<=1000000)) || { echo 'Use --photons 100..1000000' >&2;exit 2; }
if [[ -z $output ]];then output="$repo/build/validation/wls_$(date +%Y%m%d_%H%M%S)";fi
[[ $output == /* ]] || output="$PWD/$output"
[[ $output == "$repo/"* && $output != *'/../'* && ! -e $output ]] || { echo 'Output must be a NEW directory inside the repository' >&2;exit 2; }
docker inspect -f '{{.State.Running}}' "$container" | rg -q true
mkdir -p "$output"
build="/work/clas12Tags_mine/${output#"$repo/"}"
docker exec "$container" bash -lc '
set -euo pipefail
cd /work/clas12Tags_mine;source scripts/docker_env.sh
b=$1;n=$2
{ geant4-config --version;g++ --version;geant4-config --cflags;geant4-config --libs; } > "$b/environment.txt"
cd source/hitprocess/mine
read -r -a flags <<< "$(geant4-config --cflags)";read -r -a libs <<< "$(geant4-config --libs)"
case "$(geant4-config --version)" in 10.7.*) libs+=(-lG4ptl -lG4tasking);;esac
for test in geant4_straight_attenuation geant4_wls_spectrum_test;do
    g++ "${flags[@]}" -std=c++17 -O2 -I. "tests/$test.cc" mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$b/$test" "${libs[@]}" > "$b/compile-$test.log" 2>&1
done
for profile in y11 ys6 ys4;do
    conf="/work/clas12Tags_mine/experiments/mine/geant4_optics_$profile.conf"
    "$b/geant4_wls_spectrum_test" "$conf" > "$b/slab-$profile.log" 2>&1
    "$b/geant4_straight_attenuation" "$conf" "$n" 20261120 "$b/straight-$profile.csv" > "$b/straight-$profile.log" 2>&1
    tail -1 "$b/straight-$profile.log"
done
"$b/geant4_straight_attenuation" /work/clas12Tags_mine/experiments/mine/geant4_optics_y11.conf "$n" 20261121 "$b/straight-mono600.csv" 600 > "$b/straight-mono600.log" 2>&1
' mine-wls-validation "$build" "$photons"
python3 "$repo/scripts/analyze_mine_fiber_attenuation.py" --input-dir "$output" --output-dir "$output"
python3 - "$repo" "$output" <<'PY'
import hashlib,json,sys
from pathlib import Path
repo,out=map(Path,sys.argv[1:]);files=[]
files.extend((repo/'source/hitprocess/mine').glob('mine_*.h'))
files.extend([repo/'source/hitprocess/mine/mine_geant4_optics.cc',repo/'source/hitprocess/mine/mine_optical_geometry_model.cc',repo/'source/hitprocess/mine/tests/geant4_straight_attenuation.cc'])
files.extend((repo/'experiments/mine').glob('geant4_optics_y*.conf'))
files.append(repo/'experiments/mine/geant4_optics.conf')
(out/'source_manifest.json').write_text(json.dumps({str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
PY
printf 'WLS validation saved: %s\n' "$output"
