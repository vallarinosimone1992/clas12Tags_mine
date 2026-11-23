#!/usr/bin/env bash
# Stop only this repository's GEMC executable; leave Docker/X11/VNC running.
set -euo pipefail
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
list=0; pid=; run_id=; grace=5
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            cat <<'HELP'
Uso: scripts/stop_mine.sh [--list] [--pid PID | --run-id ID] [--grace SECONDI]
Senza selettore arresta tutti i GEMC di questo progetto nel container.
--list mostra soltanto PID e comando. I PID sono quelli INTERNI al container.
Invia SIGTERM, attende 5 secondi (configurabili), poi SIGKILL se necessario.
Non ferma Docker, VNC o il desktop. Un HIPO interrotto può essere incompleto.
MINE_GEMC_CONTAINER seleziona il container (default mine-gemc-vis).
HELP
            exit 0 ;;
        --list) list=1; shift ;;
        --pid|--run-id|--grace)
            [[ $# -ge 2 ]] || { echo "Valore mancante: $1" >&2; exit 2; }
            case "$1" in --pid) pid=$2 ;; --run-id) run_id=$2 ;; --grace) grace=$2 ;; esac
            shift 2 ;;
        *) echo "Opzione sconosciuta: $1" >&2; exit 2 ;;
    esac
done
[[ -z $pid || $pid =~ ^[1-9][0-9]*$ ]] && [[ $grace =~ ^[0-9]+$ ]] && ((grace<=60)) || { echo 'PID/grace non validi (grace 0..60).' >&2; exit 2; }
[[ -z $pid || -z $run_id ]] || { echo 'Scegli --pid oppure --run-id.' >&2; exit 2; }
docker exec -i "$container" python3 - "$list" "$pid" "$run_id" "$grace" <<'PY'
import os, signal, sys, time
from pathlib import Path
listing, selected, run_id, grace = sys.argv[1:]
expected='/work/clas12Tags_mine/source/gemc'
def identity(pid):
    try:
        root=Path('/proc')/str(pid)
        exe=os.readlink(root/'exe').replace(' (deleted)', '')
        if exe != expected:
            # Docker on Apple Silicon runs the x86 executable through Rosetta.
            if exe != '/run/rosetta/rosetta': return None
            first=(root/'cmdline').read_bytes().split(b'\0')[0].decode()
            target=Path(first) if first.startswith('/') else Path(os.readlink(root/'cwd'))/first
            if str(target.resolve()) != expected: return None
        # The command name can contain spaces, so parse after the last ')'.
        start=(root/'stat').read_text().rsplit(')',1)[1].split()[19]
        if run_id and ('MINE_RUN_ID='+run_id).encode() not in (root/'environ').read_bytes().split(b'\0'): return None
        return start
    except (OSError, ProcessLookupError): return None
found={}
for root in Path('/proc').iterdir():
    if not root.name.isdigit() or (selected and root.name != selected): continue
    stamp=identity(root.name)
    if stamp is None: continue
    found[int(root.name)]=stamp
    try: command=(root/'cmdline').read_bytes().replace(b'\0',b' ').decode(errors='replace')
    except OSError: command='gemc'
    print(f'GEMC PID={root.name}: {command}', flush=True)
if not found: print('Nessun processo GEMC corrispondente.'); sys.exit(0)
if listing=='1': sys.exit(0)
def send(pid, stamp, sig):
    if identity(pid)==stamp:
        try: os.kill(pid,sig)
        except ProcessLookupError: pass
for pid,stamp in found.items(): send(pid,stamp,signal.SIGTERM)
deadline=time.monotonic()+int(grace)
while time.monotonic()<deadline and any(identity(p)==s for p,s in found.items()): time.sleep(.1)
for pid,stamp in found.items():
    if identity(pid)==stamp:
        print(f'PID={pid}: SIGKILL dopo attesa.', flush=True)
        send(pid,stamp,signal.SIGKILL)
print('Arresto GEMC richiesto; desktop Docker mantenuto attivo.')
PY
