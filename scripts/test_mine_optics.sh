#!/usr/bin/env bash
# Short deterministic checks; never rebuild/replace source/gemc or MC data.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
portable=0 integration=0 photons=2000 analysis_tests=0 destination=
optical_config=${MINE_GEANT4_CONFIG:-"$repo/experiments/mine/geant4_optics.conf"}
response=${MINE_OPTICAL_RESPONSE:-"$repo/experiments/mine/responses/optical_response.dat"}
analysis_project="$repo/../simulation_time_resolution"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            cat <<'HELP'
Usage: scripts/test_mine_optics.sh [options]
  --portable           Launcher and analytic geometry checks without Docker.
  --photons N          Isolated photons per straight/bend benchmark (1..10000;
                       default 2000). Same seed, equal total centerline length.
  --integration N      Also run N particle events in EACH transport mode (1..2),
                       read HIPO and run clas12root. Requires rebuilt source/gemc.
  --optical-config F   Explicit Geant4 assumptions for integration.
  --response F         Tabulated response for integration regression.
  --analysis-tests     Full synthetic clas12root regression in a private copy.
  --analysis-project P Analysis sources (default sibling simulation_time_resolution).
  --output-dir DIR     New directory under this repository; existing paths refused.

Without --portable, compile standalone tests in the running GEMC container and
record its Geant4/compiler versions. This script never compiles GEMC itself.
All outputs have a fresh directory; no optical MC production is run. There is
no photon time cut; the isolated benchmark stops photons that escape sideways.
HELP
            exit 0 ;;
        --portable) portable=1; shift ;;
        --analysis-tests) analysis_tests=1; shift ;;
        --photons|--integration|--optical-config|--response|--analysis-project|--output-dir)
            [[ $# -ge 2 ]] || { printf 'Missing value: %s\n' "$1" >&2;exit 2; }
            case "$1" in
                --photons) photons=$2 ;; --integration) integration=$2 ;;
                --optical-config) optical_config=$2 ;; --response) response=$2 ;;
                --analysis-project) analysis_project=$2 ;; --output-dir) destination=$2 ;;
            esac
            shift 2 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2;exit 2 ;;
    esac
done
[[ $photons =~ ^[0-9]{1,5}$ && $integration =~ ^[012]$ ]] || { printf '%s\n' 'Use photons 1..10000 and integration 1..2 (or omit integration).' >&2;exit 2; }
photons=$((10#$photons));integration=$((10#$integration))
((photons>=1 && photons<=10000 && integration<=2)) || { printf '%s\n' 'Use photons 1..10000 and integration 1..2 (or omit integration).' >&2;exit 2; }
((portable==0 || integration==0)) || { printf '%s\n' '--portable and --integration are incompatible.' >&2;exit 2; }
if ((portable==0)); then
    docker info >/dev/null 2>&1 || { printf '%s\n' 'Docker is unavailable. Start Docker and the existing GEMC container, or use --portable.' >&2;exit 2; }
    [[ $(docker inspect -f '{{.State.Running}}' "$container" 2>/dev/null) == true ]] || { printf 'GEMC container is not running: %s\n' "$container" >&2;exit 2; }
fi
mkdir -p "$repo/experiments/mine/validation"
if [[ -z $destination ]]; then
    destination=$(mktemp -d "$repo/experiments/mine/validation/optics_XXXXXXXX")
else
    [[ $destination == /* ]] || destination="$PWD/$destination"
    [[ $destination == "$repo/"* && $destination != *'/../'* && ! -e $destination ]] || { printf '%s\n' '--output-dir must be a new directory within this repository.' >&2;exit 2; }
    mkdir -p "$destination"
fi
destination=$(cd "$destination" && pwd)
printf 'Validation artifacts: %s\n' "$destination"
TIMEFORMAT='wall_seconds=%R user_seconds=%U system_seconds=%S'
{
time python3 "$repo/scripts/test_mine_launch.py"
time python3 "$repo/scripts/test_mine_fiber_comparison.py"
time "${CXX:-c++}" -std=c++17 -O2 -I"$repo/source/hitprocess/mine" \
    "$repo/source/hitprocess/mine/tests/optical_geometry_model_test.cc" \
    "$repo/source/hitprocess/mine/mine_optical_geometry_model.cc" -o "$destination/geometry-model-test"
"$destination/geometry-model-test"
"${CXX:-c++}" -std=c++17 -O2 -I"$repo/source/hitprocess/mine" \
    "$repo/source/hitprocess/mine/tests/wls_spectrum_test.cc" \
    "$repo/source/hitprocess/mine/mine_optical_geometry_model.cc" -o "$destination/wls-spectrum-test"
"$destination/wls-spectrum-test" "$repo/experiments/mine/geant4_optics.conf" "$repo/experiments/mine/geant4_optics_monochromatic.conf"
cmp "$repo/experiments/mine/data/optics/y11/mine_y11_spectral_data.h" "$repo/source/hitprocess/mine/mine_y11_spectral_data.h"
"${CXX:-c++}" -std=c++17 -O2 -I"$repo/source/hitprocess/mine" \
    "$repo/source/hitprocess/mine/tests/scintillation_spectrum_test.cc" \
    "$repo/source/hitprocess/mine/mine_optical_geometry_model.cc" -o "$destination/scintillation-spectrum-test"
"$destination/scintillation-spectrum-test" "$repo/experiments/mine/geant4_optics.conf" \
    "$repo/experiments/mine/geant4_optics_fnal_extruded.conf" "$repo/experiments/mine/geant4_optics_ej200.conf"
cmp "$repo/experiments/mine/data/optics/scintillators/mine_scintillation_spectral_data.h" "$repo/source/hitprocess/mine/mine_scintillation_spectral_data.h"
"${CXX:-c++}" -std=c++17 -O2 -I"$repo/source/hitprocess/mine" \
    "$repo/source/hitprocess/mine/tests/coating_config_test.cc" \
    "$repo/source/hitprocess/mine/mine_optical_geometry_model.cc" -o "$destination/coating-config-test"
"$destination/coating-config-test" "$repo/experiments/mine/geant4_optics.conf" \
    "$repo/experiments/mine/geant4_optics_diffuse.conf" "$repo/experiments/mine/geant4_optics_tyvek.conf" "$destination"
cmp "$repo/experiments/mine/data/optics/coatings/mine_coating_spectral_data.h" "$repo/source/hitprocess/mine/mine_coating_spectral_data.h"
if ((portable==0)); then
    container_work="/work/clas12Tags_mine/${destination#"$repo/"}"
    docker exec "$container" bash -lc '
        set -euo pipefail
        cd /work/clas12Tags_mine
        source scripts/docker_env.sh
        unset MINE_RESPONSE_MODE MINE_OPTICAL_RESPONSE MINE_GEANT4_CONFIG
        build=$1;photons=$2
        { geant4-config --version; g++ --version; geant4-config --cflags; geant4-config --libs; } > "$build/environment.txt"
        cat "$build/environment.txt"
        cd source/hitprocess/mine
        read -r -a flags <<< "$(geant4-config --cflags)"
        read -r -a libs <<< "$(geant4-config --libs)"
        case "$(geant4-config --version)" in 10.7.*) libs+=(-lG4ptl -lG4tasking) ;; esac
        for test in response tabulated_response geant4_response; do
            g++ "${flags[@]}" -std=c++17 -O2 -I. "tests/${test}_test.cc" \
                mine_parameters.cc mine_response.cc mine_waveform.cc -o "$build/${test}-test" "${libs[@]}"
            "$build/${test}-test"
        done
        g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_groove_geometry_test.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$build/groove-geometry-test" "${libs[@]}"
        for profile in geant4_optics geant4_optics_groove_air geant4_optics_groove_glue; do
            "$build/groove-geometry-test" "/work/clas12Tags_mine/experiments/mine/$profile.conf" > "$build/groove-$profile.log" 2>&1
            grep -E "GROOVE_GEOMETRY_PASS" "$build/groove-$profile.log"
        done
        g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_coating_test.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$build/coating-native-test" "${libs[@]}"
        for profile in geant4_optics geant4_optics_diffuse geant4_optics_tyvek; do
            "$build/coating-native-test" "/work/clas12Tags_mine/experiments/mine/$profile.conf" > "$build/coating-$profile.log" 2>&1
            grep -E "COATING_NATIVE|checks passed" "$build/coating-$profile.log"
        done
        g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_wls_spectrum_test.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$build/wls-native-test" "${libs[@]}"
        for profile in geant4_optics geant4_optics_monochromatic; do
            "$build/wls-native-test" "/work/clas12Tags_mine/experiments/mine/$profile.conf" > "$build/wls-$profile.log" 2>&1
            grep -E "SPECTRAL_SLAB|checks passed" "$build/wls-$profile.log"
        done
        g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_scintillation_smoke.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$build/scintillation-smoke" "${libs[@]}"
        "$build/scintillation-smoke" /work/clas12Tags_mine/experiments/mine/geant4_optics.conf > "$build/scintillation.log" 2>&1
        tail -n 3 "$build/scintillation.log"
        for profile in fnal_extruded ej200; do
            "$build/scintillation-smoke" "/work/clas12Tags_mine/experiments/mine/geant4_optics_$profile.conf" 5 > "$build/scintillation-$profile.log" 2>&1
            grep -E "GENERATION_ONLY_SMOKE|SCINT_NATIVE" "$build/scintillation-$profile.log"
        done
        g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_fiber_benchmark.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc mine_optical_geometry_model.cc -o "$build/fiber-benchmark" "${libs[@]}"
        TIMEFORMAT="wall_seconds=%R user_seconds=%U system_seconds=%S"
        for mode in straight bend; do
            { time "$build/fiber-benchmark" "$mode" "$photons" 871209 "$build/$mode.csv" 100 90; } > "$build/$mode.log" 2>&1
            tail -n 5 "$build/$mode.log"
        done
        for test in hipo_output_test hipo_optical_check; do
            g++ -std=c++17 -O2 -I"$HIPO/include" "tests/$test.cc" \
                -L"$HIPO/lib" "-Wl,-rpath,$HIPO/lib" -lhipo4 -llz4 -o "$build/$test"
        done
    ' mine-validation "$container_work" "$photons"
    python3 "$repo/scripts/compare_mine_fiber_benchmarks.py" --straight "$destination/straight.csv" \
        --bend "$destination/bend.csv" --generated "$photons" --paired-launches --output "$destination/fiber-summary.json"
fi
if ((integration>0 || analysis_tests)); then
    python3 - "$analysis_project" "$destination" "$repo" <<'PY'
from pathlib import Path
import hashlib,shutil,sys
source,destination,repo=map(Path,sys.argv[1:]);copy=destination/'analysis-project'
for name in ('src','macro','scripts','config','tests'):shutil.copytree(source/name,copy/name)
shutil.copyfile(source/'README.md',copy/'README.md')
(copy/'data').symlink_to(repo/'experiments/mine/output',target_is_directory=True)
(destination/'analysis-source-sha256.txt').write_text(hashlib.sha256((source/'src/TimingAnalysis.cpp').read_bytes()).hexdigest()+'  '+str(source/'src/TimingAnalysis.cpp')+'\n')
PY
    if ((analysis_tests)); then time bash "$destination/analysis-project/scripts/test_analysis.sh"; fi
fi
if ((integration>0)); then
    # Inspect the compiled marker without running unrelated startup services.
    docker exec "$container" grep -aFq MINE_GEANT4_OPTICS_V1 /work/clas12Tags_mine/source/gemc || {
        printf '%s\n' 'Installed GEMC lacks MINE Geant4 optics; build it explicitly before integration.' >&2;exit 2;
    }
    for mode in geant4 tabulated; do
        input=(--optical-config "$optical_config");[[ $mode == geant4 ]] || input=(--response "$response")
        time bash "$repo/scripts/run_mine.sh" --batch --transport "$mode" "${input[@]}" \
            --card experiments/mine/mine_corner_fan.gcard "-N=$integration" '-RANDOM=571203' \
            '-BEAM_P=mu-, 1*GeV, 0*deg, 0*deg' '-BEAM_V=(0, 20, -100)mm' \
            "-OUTPUT=hipo, $container_work/$mode.hipo" > "$destination/integration-$mode.log" 2>&1
        if grep -Eq 'G4Exception|GeomNav[0-9]|GeomSolids[0-9]' "$destination/integration-$mode.log"; then
            printf 'Geant4 geometry/navigation exception in %s\n' "$destination/integration-$mode.log" >&2
            exit 1
        fi
        docker exec "$container" bash -lc '
            cd /work/clas12Tags_mine;source scripts/docker_env.sh
            "$1/hipo_output_test" "$1/$2.hipo" --mode "$2" --events "$3"
            optical_options=(--mode "$2" --events "$3")
            [[ $2 != geant4 ]] || optical_options+=(--require-both)
            "$1/hipo_optical_check" "$1/$2.hipo" "${optical_options[@]}"
        ' mine-hipo-validation "$container_work" "$mode" "$integration"
    done
    time bash "$destination/analysis-project/scripts/run_analysis.sh" --input "$destination/geant4.hipo" "$destination/tabulated.hipo" \
        --output-dir "$destination/analysis-results"
fi
printf 'Checks completed; artifacts preserved in %s\n' "$destination"
} 2>&1 | tee "$destination/validation.log"
