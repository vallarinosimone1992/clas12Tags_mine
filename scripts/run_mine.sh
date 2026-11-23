#!/usr/bin/env bash
# Host launcher. Only the optical input startup snapshot is temporary.
# Simulation output and the run log default to the host-mounted project.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
container=${MINE_GEMC_CONTAINER:-mine-gemc-vis}
table=${MINE_OPTICAL_RESPONSE:-"$repo/experiments/mine/responses/optical_response.dat"}
card=experiments/mine/mine.gcard
gui=1
open_viewer=1
options=()
explicit_gui=0
explicit_output=0
explicit_events=0
explicit_macro=0
explicit_card=0
lund=
input_override=0
explicit_ngenp=0
events=0
format=hipo
geometry=auto
transport=tabulated
optical_config=${MINE_GEANT4_CONFIG:-"$repo/experiments/mine/geant4_optics.conf"}
explicit_response=0
explicit_optical_config=0
progress_seconds=${MINE_PROGRESS_SECONDS:-5}
random_seed=auto
dry_run=0
for_help() {
    cat <<'HELP'
Uso: scripts/run_mine.sh [opzioni dello script] [opzioni GEMC]

Avvio e visualizzazione:
  -h, --help             Questo help (non richiede Docker).
  --help-gemc            Tutte le opzioni native GEMC e il loro significato;
                         interroga il binario nel container, senza simulare eventi.
  --help-physics         Tutti i parametri MINE modificabili nei file parameters.
  --dry-run              Valida input, geometria e opzioni dello script, senza Docker
                         o simulazione; non verifica tutte le opzioni native GEMC.
  --gui                  GUI GEMC e desktop Docker (default).
  --batch                Esecuzione senza GUI.
  Ctrl+C                 Arresta il GEMC di questo lancio anche nel container.
  scripts/stop_mine.sh --list  Elenca GEMC attivi; senza --list li arresta.
  --no-open              Non aprire il browser; lascia invariata la modalità GUI/batch.
  --card FILE            Gcard di partenza, relativa al progetto o assoluta sull'host.
                         Conserva generatore/fisica; la variation è scelta sotto.
  --geometry NAME        auto (default), corner_fan (alias diagonal_corner_fan),
                         parallel_fan_in, triple_serpentine, panel_double_serpentine,
                         mini_panel_spiral (solo --transport geant4).
                         auto legge la tabella in modalità tabulated; seleziona
                         la geometry del config ottico in modalità geant4.
                         Geant4: corner_fan o mini_panel_spiral, due sensori.
                         --geometry mini_panel_spiral seleziona anche il config
                         e la gcard mini se non sono specificati esplicitamente.
  --transport MODE       tabulated (default) oppure geant4: trasporto esplicito.
  --response FILE        Tabella ottica host alternativa al symlink predefinito.
                         Solo con --transport tabulated.
  --optical-config FILE  Default: Y-11, curve spettrali Kuraray e budget WLS senza duplicazioni.
                         Profili: geant4_optics_y11.conf, geant4_optics_ys6.conf,
                         geant4_optics_ys4.conf (directory experiments/mine).
                         Dettagli e benchmark: reference/wls_spectral_loss.md.
                         Mini: geant4_optics_mini_panel_spiral.conf (EJ200/Y11),
                         oppure geant4_optics_mini_panel_spiral_hamamatsu.conf.
                         sipmModel=ideal|hamamatsu; nel secondo caso PDE spettrale
                         e risposta SiPM semplificata, jitter aggiuntivo zero.
                         Spettri scintillatore: geant4_optics_fnal_extruded.conf
                         oppure geant4_optics_ej200.conf (dettagli nei file).
                         geant4_optics_monochromatic.conf conserva il modello precedente.
                         Rivestimento: coatingModel=specular|diffuse; coatingMaterial=
                         aluminized_mylar (default), fnal_tio2, tyvek, constant.
                         Profili pronti: geant4_optics_diffuse.conf (TiO2 FNAL),
                         geant4_optics_tyvek.conf; R(lambda) da fonti specifiche.
                         reflectivity scalare solo con coatingMaterial=constant.
                         Groove correnti: quadrati 2x2 mm, aperti sulla faccia z_MC=0.
                         grooveMaterial=gel|air|glue; centro fibra zMcMm=1.
                         Profili pronti: geant4_optics_groove_air.conf e
                         geant4_optics_groove_glue.conf. grooveModel=none per confronti.
                         Materiali e limiti: reference/grooves.md.
                         Proprietà e ipotesi ottiche esplicite Geant4, key=value.
                         Solo con --transport geant4; default geant4_optics.conf
                         in experiments/mine. Non richiede alcuna tabella MC.

Generatore (le opzioni native prevalgono sulla gcard):
  -N=NUM                 Numero di eventi. In GUI il default dello script è zero;
                         in batch senza LUND vale N della gcard.
  --lund FILE            File LUND host: valida e copia l'input nel container.
                         In batch, senza -N o con -N=0, processa tutti gli eventi.
  -BEAM_P="mu-, 10*GeV, 0*deg, 0*deg"
                         Particella, impulso, theta e phi (asse +Z).
  -SPREAD_P="0*GeV, 0*deg, 0*deg"
                         Semilarghezze dell'impulso e degli angoli.
  -BEAM_V="(0, 0, -100)mm"   Centro del vertice del generatore interno.
  -SPREAD_V="(500, 500, 0)mm"
                         Semilarghezze uniformi X,Y,Z: copre il pannello 1x1 m.
                         Con DUE valori indica invece raggio e semilarghezza Z.
                         Aggiungere ', gauss' sceglie gaussiane (valori = sigma).
                         Con LUND usare i vertici del file, non SPREAD_V.
  -RANDOM=SEED           Default: seed casuale dal sistema operativo, salvato nel log
                         e nelle condizioni HIPO. Anche 0, TIME o auto scelgono
                         un nuovo seed; un intero 1..2147483646 e riproducibile.
                         Il launcher prevale su un eventuale RANDOM della gcard.
  -NGENP=NUM             Massimo numero di primarie; --lund lo ricava dal file.
  -INPUT_GEN_FILE="FORMATO, FILE"
                         Input nativo nel container, alternativa a --lund.

Output e impostazioni avanzate:
  --progress-seconds SEC  Intervallo minimo fra aggiornamenti (default 5 s).
                         Eventi completati/richiesti, eventi scritti, velocita ed ETA.
                         Aggiornamento dopo il primo evento e riepilogo a fine run;
                         durante un evento lungo attende che questo termini.
                         In GUI usa il numero di eventi scelto nel pulsante Run.
  -EVENT_VERBOSITY=NUM   Default 0: il nuovo progresso sostituisce i log di inizio
                         evento. Con NUM > 0 conserva anche i messaggi diagnostici.
  --txt                  Output diagnostico testo invece di HIPO (default).
  -OUTPUT="hipo, PATH"   Output esplicito: PATH è nel filesystem del container.
                         Per persistenza usare /work/clas12Tags_mine/...
                         Il default è HIPO univoco in experiments/mine/output/;
                         il log resta nella stessa cartella anche con -OUTPUT.
  -USE_GUI=0|1|2         Modalità nativa: batch, GUI, terminale interattivo.
  -EXEC_MACRO=FILE       Macro Geant4 nel container; sostituisce la vista standard.
  -PHYSICS="STD + FTFP_BERT"  Lista di processi fisici.
                         In modalità geant4 la fisica ottica è attivata da MINE.
  Tutte le altre opzioni GEMC sono inoltrate come argomenti separati.
  Le opzioni -help* interrogano GEMC senza avviare una simulazione.
  Non esiste un flag multithread in questa versione sequenziale di GEMC.

Risposta del rivelatore:
  tabulated usa efficienze e distribuzioni temporali complete esportate dal MC.
  geant4 costruisce core/cladding e trasporta i fotoni: le ipotesi ottiche non
  misurate vanno dichiarate nel config. Non usa velocità o trapping efficaci.
  Entrambe le modalità riutilizzano SiPM e digitalizzazione esistenti.
  Vedi experiments/mine/GEANT4_OPTICS.md e --help-physics.

Variabili d'ambiente:
  MINE_RUN_ID           ID opzionale del lancio (usato dal supervisore parallelo).
  MINE_PROGRESS_SECONDS Intervallo progresso in secondi (default 5);
                        --progress-seconds prevale sulla variabile.
  MINE_GEMC_CONTAINER    Container già avviato (default mine-gemc-vis).
  MINE_OPTICAL_RESPONSE Tabella host (default responses/optical_response.dat).
  MINE_GEANT4_CONFIG    Config host Geant4 alternativo; richiede --transport geant4.
  MINE_VIS_URL          URL del desktop Docker
                        (default http://127.0.0.1:6080/vnc_lite.html?scale=true).

Esempi:
  scripts/run_mine.sh --batch --geometry corner_fan -N=10000
  scripts/run_mine.sh --batch --transport geant4 --optical-config modello.conf -N=1
  scripts/run_mine.sh --batch -N=10000 '-SPREAD_V=(500, 500, 0)mm'
  scripts/run_mine.sh --batch --lund /percorso/eventi.lund
  scripts/run_mine.sh --help-gemc
  scripts/run_mine_parallel.sh --runs 4 --events 1000 -- --transport geant4
                         Run separate in background; --help per stato e arresto.
HELP
}
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h) for_help; exit 0 ;;
        --help-physics)
            printf '%s\n' 'Parametri per variation: nome | valore | unità | significato.' \
                'Con tabelle attive il trasporto legacy non viene applicato; consultare PHYSICS.md.'
            for parameters in "$repo"/experiments/mine/mine__parameters_*.txt; do
                printf '\n%s\n' "$parameters"
                cat "$parameters"
            done
            exit 0 ;;
        --help-gemc|-help*)
            query=$1; [[ $query != --help-gemc ]] || query=-help-all
            exec docker exec "$container" bash -lc \
                'cd /work/clas12Tags_mine; source scripts/docker_env.sh; source/gemc "$1"' mine-help "$query" ;;
        --dry-run) dry_run=1; shift ;;
        -RANDOM=*) random_seed=${1#*=}; shift ;;
        --progress-seconds) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --progress-seconds value' >&2; exit 2; }; progress_seconds=$2; shift 2 ;;
        --geometry) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --geometry value' >&2; exit 2; }; geometry=$2; shift 2 ;;
        --transport) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --transport value' >&2; exit 2; }; transport=$2; shift 2 ;;
        --response) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --response value' >&2; exit 2; }; table=$2; explicit_response=1; shift 2 ;;
        --optical-config) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --optical-config value' >&2; exit 2; }; optical_config=$2; explicit_optical_config=1; shift 2 ;;
        -VARIATION=*) printf '%s\n' 'Use --geometry to select and validate the MINE variation.' >&2; exit 2 ;;
        --card) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --card value' >&2; exit 2; }; card=$2; explicit_card=1; shift 2 ;;
        --lund) [[ $# -ge 2 ]] || { printf '%s\n' 'Missing --lund file' >&2; exit 2; }; lund=$2; shift 2 ;;
        --txt) format=txt; shift ;;
        --gui) gui=1; options+=("-USE_GUI=1"); explicit_gui=1; shift ;;
        --batch) gui=0; options+=("-USE_GUI=0"); explicit_gui=1; shift ;;
        --no-open) open_viewer=0; shift ;;
        -USE_GUI=*) gui=${1#*=}; options+=("$1"); explicit_gui=1; shift ;;
        -OUTPUT=*) output_override=${1#*=}; options+=("$1"); explicit_output=1; shift ;;
        -N=*) events=${1#*=}; explicit_events=1; shift ;;
        -NGENP=*) options+=("$1"); explicit_ngenp=1; shift ;;
        -INPUT_GEN_FILE=*) options+=("$1"); input_override=1; shift ;;
        -EXEC_MACRO=*) options+=("$1"); explicit_macro=1; shift ;;
        *) options+=("$1"); shift ;;
    esac
done
if ! python3 - "$progress_seconds" <<'CHECK_PROGRESS'
import math
import sys
try:
    value = float(sys.argv[1])
    if not math.isfinite(value) or value <= 0:
        raise ValueError("positive finite interval required")
except (ValueError, AssertionError):
    sys.exit(1)
CHECK_PROGRESS
then
    printf '%s\n' '--progress-seconds / MINE_PROGRESS_SECONDS must be a finite number > 0.' >&2
    exit 2
fi
random_seed=$(python3 "$repo/scripts/mine_random_seed.py" "$random_seed")
options+=("-RANDOM=$random_seed")
staging=$(mktemp -d "${TMPDIR:-/tmp}/mine-launch.XXXXXXXX")
trap 'rm -rf "$staging"' EXIT
case "$transport" in
    tabulated)
        [[ $explicit_optical_config == 0 ]] || { printf '%s\n' '--optical-config requires --transport geant4.' >&2; exit 2; }
        [[ -f "$table" && -r "$table" ]] || { printf 'Missing/unreadable optical response (or broken symlink): %s\n' "$table" >&2; exit 2; }
        table=$(cd "$(dirname "$table")" && pwd)/$(basename "$table")
        transport_arguments=(--response "$table") ;;
    geant4)
        if [[ $geometry == mini_panel_spiral && $explicit_optical_config == 0 && -z ${MINE_GEANT4_CONFIG:-} ]]; then
            optical_config="$repo/experiments/mine/geant4_optics_mini_panel_spiral.conf"
        fi
        [[ $explicit_response == 0 ]] || { printf '%s\n' '--response requires --transport tabulated.' >&2; exit 2; }
        [[ -f "$optical_config" && -r "$optical_config" ]] || { printf 'Missing/unreadable Geant4 optical config: %s\n' "$optical_config" >&2; exit 2; }
        optical_config=$(cd "$(dirname "$optical_config")" && pwd)/$(basename "$optical_config")
        transport_arguments=(--optical-config "$optical_config") ;;
    *) printf 'Unknown transport mode: %s (use tabulated or geant4).\n' "$transport" >&2; exit 2 ;;
esac
card_arguments=(--card "$card")
[[ $explicit_card == 1 ]] || card_arguments+=(--default-card)
python3 "$repo/scripts/mine_launch_card.py" --repo "$repo" "${card_arguments[@]}" \
    --geometry "$geometry" --transport "$transport" "${transport_arguments[@]}" --output "$staging/launch.gcard"
[[ $gui == 0 || $gui == 1 || $gui == 2 ]] || { printf '%s\n' 'USE_GUI must be 0, 1 or 2' >&2; exit 2; }
if [[ -n $lund ]]; then
    [[ $input_override == 0 ]] || { printf '%s\n' 'Choose --lund or -INPUT_GEN_FILE, not both.' >&2; exit 2; }
    [[ -f "$lund" && -r "$lund" ]] || { printf 'Missing/unreadable LUND file: %s\n' "$lund" >&2; exit 2; }
    lund=$(cd "$(dirname "$lund")" && pwd)/$(basename "$lund")
    summary=$(python3 "$repo/scripts/lund_summary.py" "$lund")
    read -r lund_events lund_particles <<< "$summary"
    [[ $events =~ ^[0-9]+$ ]] || { printf '%s\n' 'With --lund, -N must be a nonnegative integer.' >&2; exit 2; }
    events=$((10#$events))
    if (( events > lund_events )); then
        printf 'Requested %s events but LUND contains only %s.\n' "$events" "$lund_events" >&2; exit 2
    fi
    if [[ $gui == 0 && $events == 0 ]]; then events=$lund_events; fi
    if [[ $explicit_ngenp == 0 ]]; then options+=("-NGENP=$lund_particles"); fi
fi
if [[ $explicit_events == 1 || -n $lund ]]; then options+=("-N=$events"); fi
if [[ $dry_run == 1 ]]; then
    printf 'Preflight MINE valido; seed=%s; nessun processo avviato.\n' "$random_seed"
    printf 'Opzioni GEMC:'
    printf ' %q' "${options[@]}"
    printf '\n'
    exit 0
fi
output_dir="$repo/experiments/mine/output"
mkdir -p "$output_dir"
run_id=${MINE_RUN_ID:-"$(date +%Y%m%d_%H%M%S)_$$"}
[[ $run_id =~ ^[a-zA-Z0-9_.-]+$ ]] || { printf '%s\n' 'MINE_RUN_ID non valido.' >&2; exit 2; }
log="$output_dir/mine_$run_id.log"
if [[ $explicit_gui == 0 ]]; then options+=("-USE_GUI=$gui"); fi
if [[ $explicit_output == 0 ]]; then
    options+=("-OUTPUT=$format, /work/clas12Tags_mine/experiments/mine/output/mine_$run_id.$format")
    printf 'Output persistente: %s/mine_%s.%s\n' "$output_dir" "$run_id" "$format"
else
    printf 'Output richiesto: %s\n' "$output_override"
fi
printf 'Log persistente: %s\n' "$log"
# docker exec does not reliably forward terminal SIGINT to the remote process.
interrupt_run() {
    trap '' INT TERM
    printf '\nArresto GEMC del run %s...\n' "$run_id" >&2
    "$repo/scripts/stop_mine.sh" --run-id "$run_id" --grace 2 >&2 || true
    exit 130
}
trap interrupt_run INT TERM
if [[ $gui != 0 ]]; then
    if [[ $explicit_events == 0 && -z $lund ]]; then options+=("-N=0"); fi
    if [[ $explicit_macro == 0 ]]; then options+=("-EXEC_MACRO=/opt/gemc-visualization/mine-view.mac"); fi
    viewer=${MINE_VIS_URL:-'http://127.0.0.1:6080/vnc_lite.html?scale=true'}
    printf 'Visualizzazione Docker: %s\n' "$viewer"
    if [[ $open_viewer == 1 ]]; then
        if command -v open >/dev/null 2>&1; then
            open "$viewer" || printf '%s\n' 'Apri il link della visualizzazione nel browser.' >&2
        elif command -v xdg-open >/dev/null 2>&1; then
            xdg-open "$viewer" >/dev/null 2>&1 || printf '%s\n' 'Apri il link della visualizzazione nel browser.' >&2
        fi
    fi
fi
# Follow host symlinks through a streaming archive; the LUND file may also live
# outside the directory mounted by Docker. No production input is modified.
archive_files=(launch.gcard)
if [[ $transport == tabulated ]]; then
    ln -s "$table" "$staging/response.dat"
    archive_files+=(response.dat)
else
    ln -s "$optical_config" "$staging/geant4_optics.conf"
    archive_files+=(geant4_optics.conf)
fi
if [[ -n $lund ]]; then
    ln -s "$lund" "$staging/events.lund"
    archive_files+=(events.lund)
fi
{
    printf 'MINE transport: %s\n' "$transport"
    printf 'MINE random seed: %s (CLHEP MixMax; salvato nelle condizioni di simulazione)\n' "$random_seed"
    if [[ $transport == tabulated ]]; then
        printf 'MINE host optical response: %s\n' "$table"
        ls -l "$table"
    else
        printf 'MINE host Geant4 optical config: %s\n' "$optical_config"
        cat "$optical_config"
    fi
    if [[ -n $lund ]]; then
        printf 'LUND input: %s; events=%s; maximum particle rows=%s\n' "$lund" "$lund_events" "$lund_particles"
    fi
    # Arguments stay separate; no evaluation of user options. The existing Docker
    # desktop remains running after GEMC exits. Only the startup snapshots are removed.
    COPYFILE_DISABLE=1 tar --no-xattrs -chf - -C "$staging" "${archive_files[@]}" | docker exec -i "$container" bash -lc '
        set -euo pipefail
        export MINE_RUN_ID="$1"; shift
        export MINE_PROGRESS_SECONDS="$1"; shift
        cd /work/clas12Tags_mine
        source scripts/docker_env.sh
        snapshot=$(mktemp -d /tmp/mine-optical.XXXXXXXX)
        trap '\''rm -rf "$snapshot"'\'' EXIT
        tar --no-same-owner -xf - -C "$snapshot"
        export MINE_RESPONSE_MODE="$1"; shift
        export MINE_GEANT4_SOURCE_CONFIG="$1"; shift
        unset MINE_OPTICAL_RESPONSE MINE_GEANT4_CONFIG
        if [[ $MINE_RESPONSE_MODE == tabulated ]]; then
            export MINE_OPTICAL_RESPONSE="$snapshot/response.dat"
        else
            export MINE_GEANT4_CONFIG="$snapshot/geant4_optics.conf"
            if ! grep -aFq MINE_GEANT4_OPTICS_V1 source/gemc; then
                printf "%s\n" "GEMC binary lacks explicit MINE optics: rebuild source/gemc before --transport geant4." >&2
                exit 2
            fi
        fi
        card="$snapshot/launch.gcard"
        input_options=()
        if [[ -f "$snapshot/events.lund" ]]; then
            input_options+=("-INPUT_GEN_FILE=LUND, $snapshot/events.lund")
        fi
        source/gemc "$card" "${input_options[@]}" "$@"
    ' mine-launch "$run_id" "$progress_seconds" "$transport" "$optical_config" "${options[@]}"
} 2>&1 | tee "$log"
