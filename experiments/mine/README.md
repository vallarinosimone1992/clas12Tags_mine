> **Aggiornamento groove e output MC (12 novembre 2026):** i preset espliciti correnti usano groove aperti 2×2 mm sulla faccia z_MC=0, centro fibra z_MC=1 mm e riempimento `grooveMaterial=gel|air|glue` (default gel). Le descrizioni sotto del contatto diretto plastico/cladding e di z_MC=.5 mm rimangono riferimenti storici senza groove. Nuova banca `MC::WLS` per ogni assorbimento e quattro campi fibra in `MINE::optical` (ora 15 campi). [Dettagli, fonti e selezione](../../reference/grooves.md).

# Standalone MINE response

MINE has its own sensitive detector, HitProcess and output bank. Geant4 tracks
particles in a 1000 x 1000 x 10 mm plastic panel. The default `tabulated`
transport samples the existing optical response; `--transport geant4` enables
explicit optical photons, for corner_fan with Y-11/YS-6/YS-4 single cladding and
two sensors. The generic air world is 2 m per side, without CLAS12 geometry or fields.

The default is now an extruded-scintillator working model, Kuraray Y-11(200)
round single-clad WLS fiber, and Hamamatsu S13360-3075CS at the nominal operating
point. See [PHYSICS.md](PHYSICS.md) for definitions, sources and limitations.
See [GEANT4_OPTICS.md](GEANT4_OPTICS.md) for explicit transport, material assumptions,
photon truth and the validation status.
The completed implementation, quantitative checks, modified files and remaining
physical assumptions are recorded in [VALIDATION_20261104.md](VALIDATION_20261104.md).

## Avanzamento della simulazione

Ogni run mostra eventi completati/richiesti, percentuale, eventi scritti,
tempo trascorso, velocita media ed ETA (tempo residuo stimato). Il messaggio
sulla risposta ottica ideale viene stampato una sola volta per processo.
Il primo aggiornamento arriva dopo il primo evento, poi con intervallo minimo
di 5 s, sempre al termine di un evento; un riepilogo viene stampato a fine run.
Cambiare la frequenza con `scripts/run_mine.sh --progress-seconds 10 ...`
oppure `MINE_PROGRESS_SECONDS=10` (anche per il lancio diretto di GEMC).

Il totale richiesto e letto dalla run Geant4 effettiva, anche con Run della GUI
o `/run/beamOn`; i contatori ripartono a ogni run e viene riportato anche il
totale scritto nel processo. Gli eventi filtrati/saltati contano nel progresso
ma non negli eventi scritti; gli eventi abortiti non sono dichiarati completati.
Scritti indica eventi consegnati al writer, che puo bufferizzare: il file HIPO
viene finalizzato alla chiusura normale di GEMC. L'ETA usa il tempo reale medio
per evento e puo variare con posizione e costo del trasporto ottico; non include
la preparazione iniziale o la chiusura finale. Non c'e un aggiornamento nel mezzo
di un singolo evento. I dettagli nativi restano disponibili con
`-EVENT_VERBOSITY=1` (e `-PRINT_EVENT=...`). La fisica non cambia.

## Run parallele e seed

`scripts/run_mine_parallel.sh --runs N --events M -- --transport geant4`
avvia N processi separati in background, ciascuno con M eventi e seed distinto.
Non serve aggiungere `&` o `nohup`; `--jobs J` limita i processi simultanei.
HIPO e log separati, manifest con seed/comandi/stati e lista per analisi
vengono scritti in una nuova sottocartella di `experiments/mine/output/`.
[Guida completa, stato, stop selettivo e analisi multifile](PARALLEL_RUNS.md).

Anche il lancio singolo sceglie ora un seed casuale dal sistema operativo se
`-RANDOM` e assente oppure vale `0`, `TIME` o `auto`. `-RANDOM=23456` conserva
una run riproducibile. Il numero effettivo e nel log e nelle condizioni HIPO;
la scelta del launcher prevale su RANDOM della gcard. GEMC usa CLHEP/MixMax,
non ROOT/TRandom3; non viene modificata la semantica del binario nativo.
[Provenienza e limiti](../../reference/random_seeds.md).

## Active optical table (default)

All MINE cards now use `mine_use_optical_table=1`. The stable selector is
`experiments/mine/responses/optical_response.dat`. Change only this symlink to
select another **self-contained export for the same panel**, then start a new
GEMC process. Layout, fiber count, clear/WLS type, WLS profile, optical efficiency,
attenuation choices, conditional transport times, wavelength proxy and missing
WLS decay are read from the table. The legacy fiber routing is not constructed.
The current active response is the 15-fiber corner fan with 1000 mm plastic and
3500 mm fiber attenuation along the actual optical paths. The latter remains a
working path-length proxy, not a conversion of the catalog axial attenuation.

From the **host**, in this repository:

```sh
./scripts/run_mine.sh                    # open the Docker desktop and GEMC GUI
./scripts/run_mine.sh --batch -N=100     # run without graphics
```

The default launch opens the noVNC desktop in the host browser and starts GEMC
with graphics enabled; without an explicit `-N` it generates no events until
requested in the GUI. `-N=100` generates those initial events and leaves the GUI
open. Close GEMC to finish the run; the Docker desktop/container stays running.
`--batch` (or `-USE_GUI=0`) disables graphics. `--no-open` keeps graphics but does
not open a browser tab. `MINE_VIS_URL` overrides the default desktop URL.

Output defaults to a unique `.hipo` file in `experiments/mine/output/`, a persistent
host-mounted project directory, with a corresponding `.log`. Files remain after
GEMC exits and after the container stops or is removed. Explicit `-OUTPUT` options
are honored as given, so a user-selected `/tmp` destination is container-local.
The only file removed by the launcher at run completion is the private optical
startup snapshot described below.

The host launcher resolves the symlink and streams one startup snapshot to a
private temporary file in the running `mine-gemc-vis` container, where GEMC loads
it once. This is necessary because that container does not mount the optical MC
project. The temporary copy is automatically removed at the end; no manual copy,
container recreation or rebuild is needed when selecting another table.
`MINE_GEMC_CONTAINER` selects another running container with this repository at
`/work/clas12Tags_mine`; `MINE_OPTICAL_RESPONSE` optionally overrides the host file.
The launcher does not change source files or the selected link.

For example, select the already exported triple-serpentine response:

```sh
ln -sfn ../../../../MC_MINE_LightTrapping/responses/uniform_triple_serpe_wls_gemc.dat experiments/mine/responses/optical_response.dat
./scripts/run_mine.sh --batch -N=100
```

Use the analogous `uniform_corner_fan_wls_gemc.dat` target to return to corner fan.
Run directly inside a container only when the table target is mounted, or set
`MINE_OPTICAL_RESPONSE` there to a readable response file. Direct launches resolve
`GEMC_DATA_DIR/experiments/mine/responses/optical_response.dat` (then `GEMC` if the
former environment variable is unset). A missing/broken link is a fatal error.

Export with the updated `light_trapping_export` from the **raw** optical MC ROOT.
An older `.dat` without `gemc_contract_version=1` must be re-exported; regenerating
photons is unnecessary. The exporter configuration selects attenuation and the
Y-11/YS-6 spectral/decay profile; geometry/type are taken from the raw file.
An explicitly zero attenuation length means that loss is disabled, and GEMC
prints that choice rather than adding an independent approximation.

GEMC retains Birks, light yield, scintillation decay, coupling, SiPM and electronics
parameters. Collection/trapping efficiencies, route lengths, propagation speeds,
legacy geometry and WLS profile/decay overrides are ignored in the table branch.
Each primary PE gets independent scintillation and transport delays; a WLS decay
is added only if the table declares it missing. There is no direct-flight
subtraction. Times in the table are full optical delays and include their entire
sampled tail. PDE uses the table's emission-peak proxy, not a resolved spectrum.

Both SiPMs are simulated independently for a Poisson scintillation population.
The bank has `Ncollected0/1`, `Ntransported0/1`, `meanNpe0/1`, `Npe0/1`,
`Nfired0/1`, noise counts, `ADC0/1`, `TDC0/1`, `timingValid0/1` and
`effectivePDE0/1`. `TDCmean=(TDC0+TDC1)/2` and `TDCdifference=TDC1-TDC0` are valid
only when `bothTimingValid=1`; neither includes position-dependent calibration.
Old scalar fields still describe `mine_readout_end`, by default sensor 0.
`opticalTable`, `fiberCount`, `fiberLayout`, `fiberModel` and the exact-double
52-bit `opticalHash` file fingerprint identify the selected response in output.
Unavailable bulk/fiber distance summaries are -1. The electronics retains its
finite gate: late photons remain in sampled PE counts but need not contribute
to gated ADC/TDC. Z dependence is averaged as in the optical MC table.

Startup rejects missing/invalid metadata, unsampled cells and numerical-limit
censoring. The table must cover the complete local XY panel; a cropped grid is
not silently extrapolated. Its physical XYZ dimensions must match the Geant4 Box.
Changing the symlink selects optics for that panel; it does not resize or rotate
the Geant4 sensitive volume. Sensor numbering follows the MC. Unsupported new
WLS profiles require exporter support instead of silently using Y-11 constants.

The portable reader in `source/hitprocess/mine/optical/OpticalResponseTable.h`
is an unchanged vendored copy of the MC reader (C++17, no ROOT dependency).

## LUND to HIPO production

The default persisted output is now HIPO4, with an embedded bank dictionary and
all 58 MINE response fields (including `responseMode` and `Nscintillation`). Until a physics LUND file is available, the internal
1 GeV/c mu-minus beam remains the test source. Opening the GUI without `-N`
previews the geometry; events are produced with Run or an explicit `-N`.

For an existing LUND on the host (including outside the mounted project):

```sh
./scripts/run_mine.sh --batch --lund /path/to/events.lund
```

This processes exactly the number of complete input events, writes a unique
`experiments/mine/output/mine_<timestamp>_<pid>.hipo` plus `.log`, and preserves
input events without MINE hits. `--lund FILE -N=100` selects only the first 100
records; requesting more records than the file contains is an error. In batch
mode `-N=0` with `--lund` also means all records. In GUI mode the default remains
`-N=0` (no initial events). The read-only streaming preflight validates LUND
structure and prevents truncated records. Unless explicitly overridden, `NGENP`
is set to the maximum input multiplicity so MC primaries are not silently capped
at the framework default of 10.

The host launcher streams temporary snapshots of the optical table and LUND
through an archive into the running container. Original inputs are unchanged;
only these private startup snapshots are deleted at run completion. The file
and event count are recorded in the persistent run log. `--lund` cannot be
combined with a separate `-INPUT_GEN_FILE` option. Advanced users may pass GEMC's
native `-INPUT_GEN_FILE="LUND, /container/path"` when that file is already visible
inside Docker, with the event limit under their control.

Relevant output banks:

| HIPO bank | Contents |
|---|---|
| `RUN::config` | Run and event identifiers |
| `MC::Particle` | Primaries actually passed to Geant4; momentum GeV, vertex cm, time ns |
| `MC::Lund` | Original LUND particle rows, when LUND input is used |
| `MC::Event`, `MC::User` | LUND header and optional extra header variables |
| `MC::True` | Integrated Geant4 hit truth: deposited energy, mean local/global position, time, track/particle information and `hitn` |
| `MINE::dgt` | One row per MINE panel hit, with energy/light, both SiPMs, ADC/TDC, validity flags and optical-table identity |
| `MINE::optical` | Geant4 mode only: individual sensor arrivals, wavelength, full ancestral path, PDE decision and panel/hit association |

`MINE::dgt` has group 4600/item 2. Fields declared `Dd` in the TEXT bank are
stored as HIPO doubles (including ADC, times and `opticalHash`), and `Di` as
32-bit integers. Its schema header is generated from the canonical field list
in `geometry_source/mine/mine.pl`; rerun that generator before rebuilding when
changing fields. MINE uses local standalone detector ID 110 in
`MC::True.detector`, not an official CLAS12 detector code. Join response and truth
within each event using `hitn` and detector ID. There is no CLAS12 raster in this
standalone setup, so MINE HIPO does not query its raster CCDB calibration or
write a synthetic raster bank.

For data analysis use `ADC0/1`, `TDC0/1`, `Npe0/1`, `timingValid0/1`,
`TDCmean/TDCdifference` and `bothTimingValid`, with deposited/visible energy and
MC truth as needed. Waveform samples and individual optical photons are not
stored in this bank. Individual Geant4 arrivals are stored separately in
`MINE::optical` (4600/3), as documented in [GEANT4_OPTICS.md](GEANT4_OPTICS.md).
HIPO is a dedicated MINE pseudo-data format here; standard
CLAS12 reconstruction does not automatically reconstruct this custom detector.
To retain denominators for efficiency studies, keep `FILTER_HITS=0` (default).
`--txt` restores text diagnostic output; explicit `-OUTPUT` settings still win.

The 2026-10-21 HIPO integration test verified a two-event internal-beam run and
a separate two-record synthetic LUND fixture: a panel crossing plus an off-panel
muon. Reading back the HIPO verified all 56 fields/types, exact optical hash,
MC truth and both sensor responses, including retention of the no-hit event.
No production LUND sample was generated.

## Legacy response reference

The following original response/routing description applies to
`mine_use_optical_table=0`. It is retained for historical comparisons.

## Response chain

1. Stepwise Birks quenching, expected photon yield and exponential scintillation
   delay. Neutral or zero-length depositing steps keep their raw energy;
   `unquenchedSteps` counts charged depositing steps with zero length.
2. Pre-trapping effective capture/re-emission probability, bulk attenuation and
   bulk propagation delay. The nearest point on a fiber centerline approximates
   capture; it is not an optical collection map or ray trace.
3. WLS: **per-readout-end** trapping, attenuation and delay along the fiber route,
   coupling, WLS emission delay and emission-peak wavelength. Direct-fiber mode
   skips WLS trapping/re-emission; its collection efficiency must already account
   for acceptance into guided modes. Reference mode keeps the same collection geometry but skips fiber transport effects.
4. Interpolated wavelength-dependent PDE, Poisson primary PE, finite-cell gate
   occupancy, one-generation prompt cross-talk and in-hit dark noise. Afterpulse
   remains disabled because its probability/time are not specified by the
   datasheet. Optional ideal SiPM bypasses sensor losses and secondary noise.
5. Time-stamped avalanche pulses, sampled waveform, integral ADC and leading-edge
   TDC. Electronics defaults are unchanged. `timingValid=0`, `TDC=-1` denotes no
   threshold crossing. Waveform samples are available in C++ but not saved.

## Configuring a run

Edit `mine__parameters_default.txt`: no rebuild is required for supported TEXT
parameters. All fallback defaults are in `source/hitprocess/mine/mine_parameters.h`;
all supported names and checks are in `mine_parameters.cc`.

Main switches:

| TEXT parameter | Values |
|---|---|
| `mine_readout_mode` | 0 reference; 1 WLS (default); 2 direct fiber |
| `mine_fiber_model` | 0 Y-11(200) single clad (default); 1 future YS-6 hypothesis |
| `mine_fiber_layout` | 0 legacy point; 1 compact fan-in (default); 2 legacy double; 3 corner fan; 4 triple; 5 panel double; 6 straight |
| `mine_readout_end` | 0 or 1: one physical SiPM end per response/run |
| `ideal_SiPM` | 0 real model (default); 1 PDE=1, no finite-cell losses, dark, cross-talk or afterpulse |
| `mine_spectral_pde` | 1 Hamamatsu curve (default); 0 constant `mine_pde` |

Changing `mine_fiber_model` supplies the profile's attenuation length, decay time
and emission wavelength before explicit TEXT overrides. Do not copy those three
values into the TEXT file if they should follow profile selection automatically.
`mine_pde` alone selects a constant PDE for compatibility; explicitly setting
`mine_spectral_pde=1` selects the curve even when `mine_pde` is present.
The old `mine_wls_efficiency` key is rejected with an explanation: pre-trapping
capture/re-emission belongs in `mine_collection_efficiency`, while the conditional
per-end guided fraction is `mine_wls_trapping_efficiency`.

### Important uncalibrated quantity

`mine_collection_efficiency=0.01` remains an **arbitrary pre-trapping capture and
re-emission probability**, awaiting the optical MC. It is NOT the manufacturer's
6.2% two-ended trapping fraction. The WLS model now multiplies the separate 3.1%
per-end trapping factor. Consequently the former ~45 PE estimate is obsolete:
with the new defaults, a 2 MeV central uniform deposit gives about 2 primary PE
on average at the selected end. This is not a MINE prediction, and some events
will have no signal. Setting collection to 1 is a conditional all-light-captured
study, not a calibrated panel efficiency.

### Routing and readout

The current routing builders mirror the 2026-10-19 snapshot of
`MC_MINE_LightTrapping`; source hashes are recorded in
`source/hitprocess/mine/tests/layout_sources.md`. Its current centerline formulas
are adapted directly, without importing ray tracing or ROOT. The neighboring
project is not modified and is not a runtime dependency.

Ready-to-run cards (paths are relative to the repository root):

| GCARD | TEXT variation | Layout |
|---|---|---|
| `experiments/mine/mine_corner_fan.gcard` | `corner_fan` | 14 fibers joining opposite corners, 100 mm virtual-vertex pitch, 101 mm fillet radius for Rmin=100 mm |
| `experiments/mine/mine_triple_serpentine.gcard` | `triple_serpentine` | 3 fibers, 4 lanes each, 80 mm pitch, both ends on the lower edge |
| `experiments/mine/mine_panel_double_serpentine.gcard` | `panel_double_serpentine` | 2 fibers, panel-fitted 10-lane baseline, 100 mm pitch |
| `experiments/mine/mine_parallel_fan_in.gcard` | `parallel_fan_in` | Updated compact fan-in, 10 fibers, 100 mm pitch |

Each variation has its own `mine__parameters_<variation>.txt`, an independent
snapshot of the base physics settings. Edit that file for that card. The default
card keeps layout 1 with 5 fibers; its routing now uses the updated compact bends.
Layout 2 explicitly preserves the older manually sized double serpentine.

The new parameters are `mine_corner_fibers` (even, default 14),
`mine_corner_diagonal` (0 bottom-left to top-right; 1 top-left to bottom-right),
`mine_edge_margin`, `mine_serpentine_orientation` (0 vertical; 1 horizontal), and
`mine_auto_sensor_placement` (0 manual; 1 automatic). The corner fan is currently
restricted to square panels, as in the MC. Triple layout selection supplies pitch
80 mm and edge margin 80 mm before explicit overrides. Panel double uses pitch
100 mm and margin 100 mm by default. All curved layouts enforce minimum bend
radius and panel XY bounds; the actual Box half dimensions supply the panel size
in GEMC, and fiber Z is checked against the panel thickness.

Existing parameters are `mine_parallel_fibers`, `mine_lanes_per_fiber` (layout 2),
`mine_fiber_pitch`, `mine_fiber_center_x`, `mine_fiber_z`, `mine_straight_ymin`,
`mine_straight_ymax`, `mine_sensor0_x/y`, `mine_sensor1_x/y`, `mine_sensor_pitch`,
`mine_curve_step`, `mine_min_bend_radius`. Manual sensors for layout 5 must lie on
the physical opposite edges selected by the orientation. Corner endpoints are
fixed by the selected diagonal, as in the MC, and do not use sensor pitch.

For each depositing step, the effective model finds the nearest centerline point,
uses the straight source-to-fiber distance for bulk propagation, and the arc length
from capture to the selected end for fiber propagation. This remains an
approximation, not an optical efficiency or transport-time map.
`mine_fiber_length` adds an external tail; with layout 0 it is the full fiber
distance and `mine_readout_x/y/z` specify the legacy point in fractions of Box
half-lengths. Those point coordinates are ignored by routed layouts.

Two ends feed different SiPMs: they must not be summed into one saturating sensor.
The current bank represents the selected end. Run the other end with
`mine_readout_end=1`; simultaneous correlated dual-channel output is future work.
No fiber material volumes are inserted in Geant4 by these centerlines.

### Spectral PDE

`source/hitprocess/mine/mine_sipm_pde.h` contains the canonical digitized curve and
linear interpolation. `data/s13360_3075cs_pde.csv` is a readable copy. Approximate
absolute digitization uncertainty is one percentage point. No extrapolation is
made outside 270-900 nm. The 270 nm zero is an imposed boundary anchor.

The current light packets carry an effective wavelength: WLS uses the selected
fiber's emission peak; direct/reference modes use `mine_photon_wavelength_nm=425`
as an unmeasured scintillator-peak proxy. This is not an integral over the full
emission spectrum or a model of wavelength-dependent attenuation. PDE already
contains fill factor; do not multiply by 82% again.

Ideal mode still samples the optical primary photon statistics and emission/
transport times. It preserves the configured normalized gain, waveform and
electronics. It does not remove scintillator/fiber losses.

GEMC TEXT accepts simple `mm`, `MeV`, `ns`, `counts` units, not compound units.
Use numeric Geant4 internal units with `counts` for mm/MeV, photons/MeV, mm/ns,
ns^-1 and numerical wavelengths in nm. Unknown MINE keys and invalid values fail.

## Output

The `mine` bank includes deposited/visible energy, photon counts at each stage,
mean/sampled primary PE, fired cells, dark/cross-talk/afterpulse counts, ADC,
TDC, validity, first deposit time, weighted bulk/fiber distances, readout mode,
selected end, fiber model/layout, ideal-SiPM flag and photon-weighted PDE before
saturation. `INTEGRATEDRAW=mine` saves integrated Geant4 truth.

ADC remains a continuous normalized charge integral with no hardware bit depth.
`SIGNALVT` remains explicitly unsupported without a hardware channel map.

## Build and checks

In the existing development container, with this repository at
`/work/clas12Tags_mine`:

```sh
cd /work/clas12Tags_mine
source scripts/docker_env.sh
perl geometry_source/mine/mine.pl
(cd source && scons -j2 OPT=1)
(
  cd source/hitprocess/mine
  g++ $(geant4-config --cflags) -std=c++17 -I. tests/response_test.cc \
    mine_parameters.cc mine_response.cc mine_waveform.cc \
    -o /tmp/mine-response-test $(geant4-config --libs)
  /tmp/mine-response-test
)
cd experiments/mine
../../source/gemc mine.gcard -N=20 -OUTPUT="txt, /tmp/mine-response.txt"
```

The GCARD generates central 1 GeV/c mu-minus tracks along +Z. The default output
file is overwritten by a run; use distinct output names when comparing modes.
The unrelated RTPC static initializer still contacts CLAS12 CCDB at startup.

Tests cover the extruded defaults, Birks, bulk/fiber loss and timing, route/end
consistency, both layouts, bend rejection, profile overrides, PDE interpolation,
ideal bypass, finite cells, Poisson mean/variance and full-chain timing.

## Cross-project geometry parity test

This optional test compiles against the original optical MC. From
`source/hitprocess/mine`, set `MINE_OPTICAL_MC` to its path in the build environment:

```sh
g++ $(geant4-config --cflags) -std=c++17 -I. -I"$MINE_OPTICAL_MC/include" \
  tests/geometry_parity_test.cc mine_parameters.cc \
  "$MINE_OPTICAL_MC/src/FiberGeometry.cpp" \
  "$MINE_OPTICAL_MC/src/PanelSerpentineGeometry.cpp" \
  "$MINE_OPTICAL_MC/src/AlternativeGeometries.cpp" \
  "$MINE_OPTICAL_MC/src/Simulation.cpp" \
  -o /tmp/mine-geometry-parity $(geant4-config --libs)
/tmp/mine-geometry-parity
```

The 2026-10-19 comparison covers 12 configurations and 15222 vertices: all
centerlines agree within 1e-9 mm; fiber lengths, endpoint/sensor mapping and
opposite-end distance consistency are checked too. Original MC sources are only
needed for this optional check.

## Optical MC response table link

`responses/uniform_corner_fan_wls.dat` is a relative symbolic link to
`../../../MC_MINE_LightTrapping/responses/uniform_corner_fan_wls.dat`
(relative to this directory). Keep the two project directories side by side.
Re-exporting to the same MC filename updates the file reached by the link;
a running process that already loaded the table must reload it or restart.
This named link retains the historical unattenuated response. The active
HitProcess selector is `responses/optical_response.dat`, described above.

## Tabulated response checks

Inside the configured GEMC container, from `source/hitprocess/mine`:

```sh
g++ $(geant4-config --cflags) -std=c++17 -O2 -I. \
  tests/tabulated_response_test.cc mine_parameters.cc mine_response.cc mine_waveform.cc \
  -o /tmp/mine-tabulated-test $(geant4-config --libs)
/tmp/mine-tabulated-test
```

An optional readable `.dat` argument also validates a production table and its
per-sensor photon means. Checks cover Poisson means/variances, independent
per-PE full-tail delays, missing versus already-included WLS decay, clear/YS-6
profiles, dual-sensor timing and rejection of incompatible/incomplete tables.
The legacy `response_test.cc` explicitly disables table mode for its comparisons.
The 2026-10-21 integration check ran three corner-fan and two triple-serpentine
Geant4 events through the host launcher, changing only the active symlink between
runs and restoring the 15-fiber corner-fan table afterward. Both sensors and
combined timing fields were present in the generated output.


## Selezione della geometria e help del lanciatore

```sh
scripts/run_mine.sh --help
scripts/run_mine.sh --help-gemc     # elenco nativo completo dal binario Docker, senza eventi
scripts/run_mine.sh --help-physics  # parametri MINE, con valori/unità/descrizioni
scripts/run_mine.sh --batch --geometry corner_fan -N=10000
scripts/run_mine.sh --batch --geometry triple_serpentine --response /percorso/triple_gemc.dat -N=10000
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan --optical-config /percorso/modello.conf -N=1
```

`--geometry auto` è il default e legge `fiber_geometry` dalla tabella ottica.
Con `--transport geant4`, `auto` seleziona invece `corner_fan` senza aprire alcuna
tabella MC. `--response` si usa soltanto con `tabulated`; `--optical-config` si usa
soltanto con `geant4`. Quest'ultimo è un file `key=value` autonomo, copiato insieme
alla gcard in uno snapshot privato del container. `MINE_GEANT4_CONFIG` ne sceglie
il percorso host predefinito. Il contenuto effettivo è stampato nel log della run.
La modalità Geant4 ignora `MINE_OPTICAL_RESPONSE` e il symlink della tabella.
Sono disponibili `corner_fan` (alias `diagonal_corner_fan`), `parallel_fan_in`,
`triple_serpentine`, `panel_double_serpentine`. Lo script costruisce una gcard
temporanea a partire da `--card` (default `experiments/mine/mine.gcard`),
sostituendo soltanto la variation del detector MINE. Non modifica le gcard
originali, quindi impulso, vertici e fisica restano quelli della card di partenza.

La disposizione ottica effettiva viene dalla tabella MC, non dalla sola variation.
Una richiesta esplicita deve coincidere con la tabella selezionata tramite
`--response`, `MINE_OPTICAL_RESPONSE` oppure il symlink `optical_response.dat`.
Il lanciatore rifiuta prima di Docker le geometrie sconosciute/non disponibili,
le variation prive dei file TEXT, le tabelle incompatibili e il trasporto legacy
(`mine_use_optical_table=0`). Le altre verifiche fisiche complete della tabella
rimangono nell'hit process. Non è necessario modificare il symlink per usare
`--response`.

Per verificare il preflight senza avviare GEMC:

```sh
python3 scripts/test_mine_launch.py
```


## Spectral Y-11 in the explicit Geant4 mode

Since 2026-11-04, `--transport geant4` defaults to `spectral_y11`: dye absorption
and re-emission spectra are digitized from Herzkamp/Kuraray and the peak scale
is normalized at 430 nm. The concentration is controlled by
`wlsDyeConcentrationPpm=200` in `geant4_optics.conf`. Each true WLS interaction
can reabsorb/re-emit and adds its own 6.9 ns exponential delay. The blue
scintillation source is still the 425 nm proxy and QY remains one.

```sh
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan -N=1
# Preserve/reproduce the older monochromatic reference:
scripts/run_mine.sh --batch --transport geant4 \
  --optical-config experiments/mine/geant4_optics_monochromatic.conf -N=1
```

Source data, reproducible extraction and limitations are documented in
[data/optics/y11/README.md](data/optics/y11/README.md). No new optical MC table
is needed. The tabulated transport mode remains unchanged. A running GUI must
be restarted to load the new compiled model and config.


## Scintillator emission profiles

The explicit Geant4 mode also provides `scintillationSpectrum=fnal_extruded`
(FNAL/NICADD UV-excited fluorescence proxy) and `scintillationSpectrum=ej200`
(Eljen emission curve), in addition to the historical `monochromatic` default.
They require the spectral Y11 model; source vertices and conversion policy enter
`opticalHash`. Select a ready-to-use profile:

```sh
scripts/run_mine.sh --gui --transport geant4 --geometry corner_fan \
  --optical-config experiments/mine/geant4_optics_fnal_extruded.conf -N=1
# Or: --optical-config experiments/mine/geant4_optics_ej200.conf
```

Only the source spectrum changes. Yield, timing, composition and constant bulk
absorption remain independently configured. No absolute wavelength-dependent
bulk absorption curve has been inferred from normalized measurements or strip
attenuation. Sources, digitizations, reproducible extraction and material-model
limitations are in [data/optics/scintillators/README.md](data/optics/scintillators/README.md).
The FNAL fluorescence proxy is not a validated MIP spectrum; the EJ200 plot ends
at 500 nm. Restart any existing GUI to use the newly compiled model/config.


## Specular and diffuse outer coatings

Coating angular law and material spectrum are independent config fields:
`coatingModel=specular|diffuse`, `coatingMaterial=constant|aluminized_mylar|fnal_tio2|tyvek`.
Physical reference pairings are validated. Current default is spectral Mylar;
use `geant4_optics_diffuse.conf` for FNAL coextruded TiO2 or
`geant4_optics_tyvek.conf` for Tyvek. Example:

```sh
scripts/run_mine.sh --gui --transport geant4 --geometry corner_fan \
  --optical-config experiments/mine/geant4_optics_tyvek.conf -N=1
```

Material selection does not change fiber geometry or scintillation spectrum.
Selecting a spectral material together with scalar `reflectivity` is an error;
remove the scalar, or deliberately choose `coatingMaterial=constant`. For the
Yang2021 Tyvek/Mylar curves, `coatingReferenceAlbedo=1` explicitly approximates
the BaSO4 standard as ideal. No curve is shifted to align emission peaks.
Sources, calibration limitations and native tests are in
[data/optics/coatings/README.md](data/optics/coatings/README.md). Restart a running
GUI after rebuilding/config changes. Existing MC tables are unaffected.

The central source/decision register, including user inputs and historical
assumptions, is [reference/README.md](../../reference/README.md).

## WLS profiles and finite fiber losses (2026-11-20)

Explicit Geant4 now supports `geant4_optics_y11.conf`, `geant4_optics_ys6.conf`,
and `geant4_optics_ys4.conf`: distinct spectra/decays, common 4000-mm ordinary
ray-path absorption in core and cladding. Existing spectral configs gain the
same loss. Select with `--optical-config experiments/mine/geant4_optics_ys4.conf`.
`fiberModel` codes in HIPO: 0 Y-11, 1 YS-6, 2 YS-4. The tabulated and legacy
responses remain separate. See [physics assumptions and native validation](../../reference/wls_fibers.md).

## Current spectral fiber loss (2026-11-22)

The 4 m scalar is superseded by `fiberLossModel=kuraray_spectral` in all active
spectral configs. Ordinary core/cladding loss is identical; WLS receives the
blue excess, counted once. Y11 uses the arithmetic mean YS4/YS6 coefficient
proxy. See [physics, assumptions and validation](../../reference/wls_spectral_loss.md).
The monochromatic native test passes; the full-spectrum diagnostic gives longer
attenuation than the nominal 3-4 m, and this discrepancy is retained explicitly.


## Mini panel spiral (2026-11-23)

Native `--geometry mini_panel_spiral` selects the measured 300×500×8-mm panel,
two vertically stacked 1-mm Y-11 Non-S spirals, top 1×2-mm gel groove, raised
S in air beneath a tangent 30° Mylar tent, and two 3×3-mm SiPMs with 1-mm gel
coupling. It requires `--transport geant4`; no optical MC response table is used.

```sh
scripts/run_mine.sh --gui --transport geant4 --geometry mini_panel_spiral
scripts/run_mine.sh --batch --transport geant4 --geometry mini_panel_spiral -N=10
```

The mini gcard already generates 10-GeV muons uniformly in X/Y over the whole
rectangle along +Z. Default EJ-200 nominal yield/rise/decay are 10000/.9/2.1;
SiPM is ideal. For the existing simplified Hamamatsu response select
`--optical-config experiments/mine/geant4_optics_mini_panel_spiral_hamamatsu.conf`.
Output persists in `experiments/mine/output`. The HIPO layout code is 7 and
`entryFiber` survives the gel gap. The existing sibling corner-fan overview
analysis must be adapted before it can process this layout.

[Geometry, commands, limitations and validation](../../reference/mini_panel_spiral.md),
[material sources](../../reference/mini_panel_materials.md). In particular,
1000-mm plastic bulk absorption is provisional; the specified 30-mm bend is
tighter than non-S mechanical guidance. Validation script:
`scripts/test_mine_mini_panel.sh` (`--portable` skips Docker, `--integration 1`
adds one full event per SiPM mode using precompiled GEMC).


## Config in HIPO (2026-11-23)

New Geant4 output includes `MINE::config`: the original host optical-config
path and a full copy of its actual contents. Analysis uses the embedded copy
by default and verifies its optical hash, even if the external file has moved
or changed. Explicit `--optical-config` remains available. Old HIPO files keep
the log/snapshot/preset fallback. [Format and tests](../../reference/hipo_config.md).
