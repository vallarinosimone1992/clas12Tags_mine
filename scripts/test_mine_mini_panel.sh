#!/usr/bin/env bash
# Reproducible mini-panel checks; never rebuild GEMC or run production scans.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
compiler=${CXX:-c++}
portable=0 integration=0 destination=
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            cat <<'HELP'
Usage: scripts/test_mine_mini_panel.sh [options]
  --portable           Compile/check the analytic mini model and launcher without
                       Docker; export the sampled fiber geometry as CSV.
  --integration 1      Additionally simulate exactly one muon with each SiPM
                       preset (two events total: ideal and Hamamatsu). Requires
                       an already rebuilt GEMC; this script never rebuilds it.
  --output-dir DIR     New directory under build/validation; existing paths refused.
  -h, --help           Show this help.

Default: portable checks plus the native Geant4 geometry, navigation, material
and gel/readout tests in the running MINE_GEMC_CONTAINER (mine-gemc-vis).
The optional integration also checks the two HIPO files with the current
optical-bank checker. It does not run clas12root analysis or a production scan.
--portable and --integration are incompatible. No photon arrival-time cut is
introduced. Every run preserves logs, environment, source/config SHA256 and
copies of its input configs in a fresh build/validation directory.
HELP
            exit 0 ;;
        --portable) portable=1; shift ;;
        --integration|--output-dir)
            [[ $# -ge 2 ]] || { printf 'Missing value: %s\n' "$1" >&2; exit 2; }
            if [[ $1 == --integration ]]; then
                [[ $2 == 1 ]] || { printf '%s\n' '--integration accepts only 1 (one event per SiPM preset).' >&2; exit 2; }
                integration=1
            else destination=$2
            fi
            shift 2 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done
((portable==0 || integration==0)) || { printf '%s\n' '--portable and --integration are incompatible.' >&2; exit 2; }
if ((portable==0)); then
    docker info >/dev/null 2>&1 || { printf '%s\n' 'Docker unavailable; start it or use --portable.' >&2; exit 2; }
    [[ $(docker inspect -f '{{.State.Running}}' "$container" 2>/dev/null) == true ]] || {
        printf 'GEMC container is not running: %s\n' "$container" >&2; exit 2;
    }
fi
# Resolve paths before mkdir, and reject symlinks escaping the validation tree.
destination=$(python3 - "$repo" "$destination" <<'PY'
from pathlib import Path
import sys, tempfile
repo=Path(sys.argv[1]).resolve()
base=repo/'build/validation'
base.mkdir(parents=True,exist_ok=True)
if not sys.argv[2]:
    print(tempfile.mkdtemp(prefix='mini_panel_',dir=base))
else:
    path=Path(sys.argv[2])
    if not path.is_absolute():path=Path.cwd()/path
    path=path.resolve()
    if not path.is_relative_to(base.resolve()) or path==base.resolve() or path.exists():
        raise SystemExit('--output-dir must be a new directory under this repository/build/validation')
    path.mkdir(parents=True)
    print(path)
PY
)
printf 'Mini-panel validation artifacts: %s\n' "$destination"
TIMEFORMAT='wall_seconds=%R user_seconds=%U system_seconds=%S'
{
python3 - "$repo" "$destination" "$portable" "$integration" "$compiler" <<'PY'
from pathlib import Path
import datetime,hashlib,json,platform,shutil,subprocess,sys
repo,dest=map(Path,sys.argv[1:3])
sources=set((repo/'source/hitprocess/mine').glob('*.h'))
sources.update((repo/'source/hitprocess/mine').glob('*.cc'))
for name in ('tests/mini_spiral_model_test.cc','tests/geant4_mini_spiral_test.cc','tests/hipo_optical_check.cc'):
    sources.add(repo/'source/hitprocess/mine'/name)
for name in ('scripts/test_mine_mini_panel.sh','scripts/test_mine_launch.py','scripts/mine_launch_card.py',
             'scripts/run_mine.sh','scripts/docker_env.sh','scripts/mine_random_seed.py',
             'source/physics/PhysicsList.cc','source/src/MDetectorConstruction.cc'):
    sources.add(repo/name)
inputs=dest/'inputs';inputs.mkdir()
for name in ('geant4_optics_mini_panel_spiral.conf','geant4_optics_mini_panel_spiral_hamamatsu.conf',
             'mine_mini_panel_spiral.gcard','mine__geometry_mini_panel_spiral.txt',
             'mine__hit_mini_panel_spiral.txt','mine__parameters_mini_panel_spiral.txt','mine__bank.txt'):
    path=repo/'experiments/mine'/name
    sources.add(path);shutil.copy2(path,inputs/name)
original=repo.parent/'MC_MINE_LightTrapping/config/mini_panel_spiral.conf'
if original.is_file():
    sources.add(original);shutil.copy2(original,inputs/'source_mc_mini_panel_spiral.conf')
entries=[]
for path in sorted(sources):
    label=str(path.relative_to(repo)) if path.is_relative_to(repo) else str(path)
    entries.append({'path':label,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
def command(args):
    result=subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    return {'command':args,'exit_code':result.returncode,'output':result.stdout}
environment={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
             'platform':platform.platform(),'python':sys.version,
             'portable':bool(int(sys.argv[3])),'integration_events_per_preset':int(sys.argv[4]),
             'compiler':command([sys.argv[5],'--version']),
             'bash':command(['bash','--version']),
             'git_head':command(['git','-C',str(repo),'rev-parse','HEAD'])}
(dest/'host-environment.json').write_text(json.dumps(environment,indent=2)+'\n')
(dest/'source-config-manifest.json').write_text(json.dumps({'files':entries},indent=2)+'\n')
(dest/'source-config-sha256.txt').write_text(''.join(f'{entry["sha256"]}  {entry["path"]}\n' for entry in entries))
print(f'SHA256 manifest: {len(entries)} source/config inputs')
PY
time python3 "$repo/scripts/test_mine_launch.py" 2>&1 | tee "$destination/launcher.log"
time "$compiler" -std=c++17 -O2 -I"$repo/source/hitprocess/mine" \
    "$repo/source/hitprocess/mine/tests/mini_spiral_model_test.cc" \
    "$repo/source/hitprocess/mine/mine_mini_spiral_model.cc" \
    -o "$destination/mini-model-test" 2>&1 | tee "$destination/portable-compile.log"
time "$destination/mini-model-test" "$destination/fiber-centerlines.csv" 2>&1 | tee "$destination/portable-model.log"
[[ -s "$destination/fiber-centerlines.csv" ]] || { printf '%s\n' 'Mini model did not export geometry CSV.' >&2; exit 1; }
if ((portable==0)); then
    relative=${destination#"$repo/"}
    container_work="/work/clas12Tags_mine/$relative"
    docker exec "$container" bash -lc '
        set -euo pipefail
        cd /work/clas12Tags_mine
        source scripts/docker_env.sh
        unset MINE_RESPONSE_MODE MINE_OPTICAL_RESPONSE MINE_GEANT4_CONFIG
        build=$1;integration=$2
        {
            date -u
            uname -a
            geant4-config --version
            g++ --version
            geant4-config --cflags
            geant4-config --libs
            printf "HIPO=%s\n" "$HIPO"
        } > "$build/container-environment.txt"
        cat "$build/container-environment.txt"
        cd source/hitprocess/mine
        read -r -a flags <<< "$(geant4-config --cflags)"
        read -r -a libs <<< "$(geant4-config --libs)"
        case "$(geant4-config --version)" in 10.7.*) libs+=(-lG4ptl -lG4tasking) ;; esac
        TIMEFORMAT="wall_seconds=%R user_seconds=%U system_seconds=%S"
        { time g++ "${flags[@]}" -std=c++17 -O2 -I. tests/geant4_mini_spiral_test.cc \
            mine_geant4_optics.cc mine_mini_geant4.cc mine_mini_spiral_model.cc \
            mine_optical_geometry_model.cc mine_geant4_tracking.cc \
            -o "$build/mini-native-test" "${libs[@]}"; } > "$build/native-compile.log" 2>&1
        export MINE_RESPONSE_MODE=geant4
        export MINE_GEANT4_CONFIG="$build/inputs/geant4_optics_mini_panel_spiral.conf"
        { time "$build/mini-native-test" "$MINE_GEANT4_CONFIG"; } > "$build/native-mini.log" 2>&1
        grep -E "MINI_NAVIGATION_PASS|MINI_COUPLING|MINI_NATIVE_PASS" "$build/native-mini.log"
        if ((integration)); then
            g++ -std=c++17 -O2 -I"$HIPO/include" tests/hipo_optical_check.cc \
                -L"$HIPO/lib" "-Wl,-rpath,$HIPO/lib" -lhipo4 -llz4 \
                -o "$build/hipo-optical-check" > "$build/hipo-checker-compile.log" 2>&1
        fi
    ' mine-mini-validation "$container_work" "$integration"
fi
if ((integration)); then
    docker exec "$container" bash -lc '
        set -euo pipefail
        binary=/work/clas12Tags_mine/source/gemc
        [[ -x "$binary" ]]
        grep -aFq MINE_GEANT4_OPTICS_V1 "$binary"
        grep -aFq mini_panel_spiral "$binary"
        sha256sum "$binary"
    ' > "$destination/gemc-binary-sha256.txt" || {
        printf '%s\n' 'Installed GEMC lacks compiled mini optics; rebuild it explicitly before --integration 1.' >&2; exit 2;
    }
    for sipm in ideal hamamatsu; do
        config="$destination/inputs/geant4_optics_mini_panel_spiral.conf"
        expected_ideal=1
        if [[ $sipm == hamamatsu ]]; then
            config="$destination/inputs/geant4_optics_mini_panel_spiral_hamamatsu.conf"
            expected_ideal=0
        fi
        printf 'Integration: one event, SiPM=%s\n' "$sipm"
        { time bash "$repo/scripts/run_mine.sh" --batch --transport geant4 --geometry mini_panel_spiral \
            --optical-config "$config" --card experiments/mine/mine_mini_panel_spiral.gcard \
            '-N=1' '-RANDOM=20261123' '-BEAM_V=(0, 20, -100)mm' '-SPREAD_V=(0, 0, 0)mm' \
            "-OUTPUT=hipo, $container_work/$sipm.hipo"; } > "$destination/integration-$sipm.log" 2>&1
        if grep -Eq 'G4Exception|GeomNav[0-9]|GeomSolids[0-9]' "$destination/integration-$sipm.log"; then
            printf 'Geant4 geometry/navigation exception in %s\n' "$destination/integration-$sipm.log" >&2
            exit 1
        fi
        docker exec "$container" bash -lc '
            set -euo pipefail
            cd /work/clas12Tags_mine
            source scripts/docker_env.sh
            "$1/hipo-optical-check" "$1/$2.hipo" --fiber-layout 7 --ideal-sipm "$3" \
                --fiber-model 0 --mode geant4 --events 1 --require-both
        ' mine-mini-hipo "$container_work" "$sipm" "$expected_ideal" \
            2>&1 | tee "$destination/hipo-$sipm.log"
    done
fi
printf 'MINI_PANEL_VALIDATION_PASS; artifacts preserved in %s\n' "$destination"
} 2>&1 | tee "$destination/validation.log"
