#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
photons=6000;output=
while (($#));do
 case "$1" in
  --help|-h) cat <<'HELP'
Usage: scripts/test_mine_wls_spectral_loss.sh [--photons N] [--output-dir NEW_DIR]
Native Geant4 straight-fiber spectral checks, Y11/YS6/YS4.
6000 photons/test by default; seeds fixed and recorded in commands.
Monochromatic axial core launch: 500, 505.125, 510, 520, 550, 600, 610, 650, 690 nm;
additional YS6 at 490 nm, and cladding at 500/550 nm. WLS remains active.
Unshifted primary and complete WLS lineage are counted separately.
Also record the full-spectrum guided-cone response, without forcing any global L.
No photon time cut. Output CSV/log/JSON/PNG and source/config snapshots.
Requires the running GEMC container, C++17, Python numpy/matplotlib.
HELP
  exit 0;;
  --photons|--output-dir) [[ $# -ge 2 ]]||exit 2;if [[ $1 == --photons ]];then photons=$2;else output=$2;fi;shift 2;;
  *) echo "Unknown option: $1" >&2;exit 2;;
 esac
done
[[ $photons =~ ^[0-9]+$ ]] && ((photons>=100 && photons<=1000000)) || { echo 'Use photons 100..1000000' >&2;exit 2; }
[[ -n $output ]] || output="$repo/build/validation/wls_spectral_$(date +%Y%m%d_%H%M%S)"
[[ $output == /* ]] || output="$PWD/$output"
[[ $output == "$repo/"* && $output != *'/../'* && ! -e $output ]] || { echo 'Output must be a new repository directory' >&2;exit 2; }
[[ $(docker inspect -f '{{.State.Running}}' "$container") == true ]]
mkdir -p "$output"
for profile in y11 ys6 ys4;do cp "$repo/experiments/mine/geant4_optics_$profile.conf" "$output/config-$profile.conf";done
for profile in ys6 ys4;do cp "$repo/experiments/mine/data/optics/kuraray_wls/${profile}_transmission.csv" "$output/source-$profile-transmission.csv";done
b="/work/clas12Tags_mine/${output#"$repo/"}"
docker exec "$container" bash -lc '
set -euo pipefail
cd /work/clas12Tags_mine;source scripts/docker_env.sh
b=$1;n=$2
{ geant4-config --version;g++ --version;geant4-config --cflags;geant4-config --libs; } > "$b/environment.txt"
cd source/hitprocess/mine
read -r -a flags <<< "$(geant4-config --cflags)";read -r -a libs <<< "$(geant4-config --libs)"
case "$(geant4-config --version)" in 10.7.*)libs+=(-lG4ptl -lG4tasking);;esac
for item in geant4_straight_attenuation geant4_wls_spectrum_test;do
 g++ "${flags[@]}" -std=c++17 -O2 -I. "tests/$item.cc" mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$b/$item" "${libs[@]}" > "$b/compile-$item.log" 2>&1 || { tail -70 "$b/compile-$item.log";exit 1; }
done
g++ -std=c++17 -O2 -I. tests/fiber_loss_test.cc mine_optical_geometry_model.cc -o "$b/fiber-loss-unit" > "$b/compile-fiber-loss-unit.log" 2>&1
"$b/fiber-loss-unit" "$b/config-y11.conf" "$b/config-ys6.conf" "$b/config-ys4.conf" "$b/loss_components.csv" > "$b/fiber-loss-unit.log" 2>&1
for profile in y11 ys6 ys4;do
 conf="$b/config-$profile.conf"
 "$b/geant4_wls_spectrum_test" "$conf" > "$b/slab-$profile.log" 2>&1
 for lambda in 500 505.125 510 520 550 600 610 650 690;do
  "$b/geant4_straight_attenuation" "$conf" "$n" 20261122 "$b/mono-$profile-core-$lambda.csv" "$lambda" axial > "$b/mono-$profile-core-$lambda.log" 2>&1
 done
 for lambda in 500 550;do
  "$b/geant4_straight_attenuation" "$conf" "$n" 20261123 "$b/mono-$profile-cladding-$lambda.csv" "$lambda" cladding_axial > "$b/mono-$profile-cladding-$lambda.log" 2>&1
 done
 "$b/geant4_straight_attenuation" "$conf" "$n" 20261124 "$b/broadband-$profile.csv" > "$b/broadband-$profile.log" 2>&1
 tail -1 "$b/broadband-$profile.log"
done
"$b/geant4_straight_attenuation" "$b/config-ys6.conf" "$n" 20261122 "$b/mono-ys6-core-490.csv" 490 axial > "$b/mono-ys6-core-490.log" 2>&1
' mine-spectral-check "$b" "$photons"
python3 "$repo/scripts/analyze_mine_spectral_transmission.py" --input-dir "$output" --output-dir "$output"
python3 - "$repo" "$output" <<'PY'
import hashlib,json,sys
from pathlib import Path
r,o=map(Path,sys.argv[1:]);files=list((r/'source/hitprocess/mine').glob('mine_*.h'))+list((r/'source/hitprocess/mine/tests').glob('*fiber*'))
files += [r/'source/hitprocess/mine/mine_geant4_optics.cc',r/'source/hitprocess/mine/mine_optical_geometry_model.cc',r/'source/hitprocess/mine/tests/geant4_straight_attenuation.cc',r/'source/hitprocess/mine/tests/geant4_wls_spectrum_test.cc',r/'scripts/analyze_mine_spectral_transmission.py',r/'scripts/test_mine_wls_spectral_loss.sh']
(o/'source_manifest.json').write_text(json.dumps({str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()},indent=2)+'\n')
PY
printf 'Spectral validation saved: %s\n' "$output"
