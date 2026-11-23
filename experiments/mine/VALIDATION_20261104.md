# MINE: explicit optical transport, validation completed 2026-11-04

The selectable Geant4 branch is implemented and validated for the declared
phase-1 model: corner_fan, 15 Y-11(200) single-cladding fibers and two ideal
readouts. The existing tabulated branch remains the default and passed its
particle → HIPO → clas12root regression. No long production was run.

## Use

From the repository root, with `mine-gemc-vis` running:

```sh
# Existing tabulated response.
scripts/run_mine.sh --batch --transport tabulated --geometry corner_fan -N=1

# Explicit transport; independent of the MC optical table.
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan -N=1 -RANDOM=571203

# Change explicit assumptions without editing the default configuration.
scripts/run_mine.sh --batch --transport geant4 --optical-config /path/to/model.conf -N=1

# Portable checks, or short standalone Geant4 checks.
scripts/test_mine_optics.sh --portable --analysis-tests
scripts/test_mine_optics.sh --photons 2000
# Optional: one particle event per mode, HIPO checks and clas12root.
scripts/test_mine_optics.sh --photons 2000 --integration 1
```

`--help`, `--help-physics` and [GEANT4_OPTICS.md](GEANT4_OPTICS.md) describe
selection, compatibility, configuration and explicit rebuild commands. The
test runner never rebuilds GEMC implicitly, refuses existing output directories
and caps integration at two events per mode. The launcher rejects unsupported
optical geometries, profiles and sensor counts, and checks the compiled binary
marker before explicit transport. Output and logs use new unique names.

## Actual environment and builds

The running container was queried directly: Geant4 **10.7.4**, GCC **11.5.0**,
CLHEP **2.4.7.1**, HIPO **4.5.0**. Installed APIs were checked and the application
compiled successfully with SCons/C++17. Details are preserved in
[environment.txt](validation/optics_KIVcYA5y/environment.txt).

The previous executable is preserved in
`validation/pre-optical-build.p7wJ4KgW/gemc`, with the initial full build log.
The final exterior-coating fix built successfully in
[outer-coating-build.UO2kSxf1/build.log](validation/outer-coating-build.UO2kSxf1/build.log).
All prior user changes, MC sources, selected table and existing data/results
were preserved. Failed diagnostic outputs were retained separately and were
not used as completed analysis input.

## Geometry, fiber transport and distributions

| Check | Observed result |
| --- | --- |
| Updated reference-MC routing parity | Maximum centerline discrepancy 4.26326e-14 mm; exact arc versus 2 mm chord length difference ≤0.00245933 mm |
| Physical panel geometry | All 15 claddings, 15 cores and 2 readouts passed sampled overlap checks; no final geometry warnings |
| Straight fiber, 2000 isotropic photons | 132/131 core arrivals at ends 0/1; equal-length reference 257.079632679 mm |
| R=100 mm, 90° bend, 2000 photons | 132/124 arrivals; end-1 bend/straight ratio 0.946565 ±0.019654, paired delta-method error |
| Flight-time check | Maximum discrepancy from integral(n ds/c) below 1.8e-15 ns |
| Matched reference MC, 2000 photons/case | Straight 140/130, bend 140/128; zero numerical censoring |
| Long reference launch replay | 372795 steps, 60.517679918 m, 320.965749812 ns; differences from the identical MC launch −8.97e-7 mm and −5.13e-9 ns |

The straight/bend checks preserve complete empirical time/path CDFs, quantiles,
binomial errors and exact-binomial intervals. The independent MC and Geant4
ensembles have different individual launches; their small-sample mean times
are sensitive to rare skew rays. No general numerical Jones equivalence is
claimed. The exact long-ray replay verifies the absence of the old 100/10000
step limits and of a transport-time gate.

Artifacts:
[final standalone suite](validation/optics_KIVcYA5y/validation.log),
[Geant4/MC comparison](validation/fiber_Nov3_XqivKzTB/geant4_vs_mc.json),
[long replay comparison](validation/fiber_Nov3_XqivKzTB/long_replay_comparison.json).

## Response and regression checks

Eight launcher tests and six ECDF/binomial comparator tests passed. Existing
effective-response tests gave PE mean 49.9563 and variance 50.3824. Tabulated
tests passed dual-sensor bookkeeping, complete timing tails, WLS handling and
compatibility rejection. The explicit-arrival test preserves exact ideal
counts/times and uses one Bernoulli PDE: 200 photons at PDE 0.25 over 5000 trials
gave mean 49.8884 and variance 37.6879, against expectations 50 and 37.5.

The independent generation-only regression using the actual material builder
gave 14095 scintillation photons against mean 14156.5. That diagnostic explicitly
discards optical children after counting them and makes no collection prediction.
It protects against the material-table ownership bug found during integration.

The updated clas12root analysis passed its 12000-event synthetic regression,
including 10800 deposits, empty-event retention, zero/default jitter handling,
selection and overwrite protection. The corrected mean-time sigma was
0.435700 ns against the synthetic Gaussian expectation 0.430116 ns. These
synthetic numbers validate analysis, not MINE timing resolution.

A real tabulated muon passed HIPO and clas12root with both times valid,
58 correctly typed response fields, exact table hash 3103114980582929 and
zero tracked-optical rows. Artifacts:
[tabulated analysis](validation/tabulated_bWuIXs7t/analysis-results/tabulated/summary.json).

## Final particle → optical photons → HIPO → clas12root

The final run used two 1 GeV/c muons, vertex `(0,20,-100) mm`, seed 571203 and
the active explicit configuration. `MINE_OPTICAL_RESPONSE` deliberately pointed
to `/missing/mc-table.dat`; the run completed without reading a response table.

| Quantity | Event 1 | Event 2 |
| --- | ---: | ---: |
| Particle deposited energy (MeV) | 1.648327540 | 1.586732585 |
| Scintillator visible energy (MeV) | 1.614053440 | 1.555071389 |
| Generated scintillation photons | 12798 | 12495 |
| Sensor 0 arrivals = ideal PE | 283 | 284 |
| Sensor 1 arrivals = ideal PE | 275 | 272 |
| First particle deposition t0 (ns) | 0.32200399748 | 0.32200399770 |
| TDC0 / TDC1 (ns) | 4.370766646 / 4.845345647 | 4.888243522 / 5.228456547 |

The 25293 generated photons produced 567/547 total sensor arrivals. Both sensors
have valid timing in both events. Updated HIPO checks passed positive actual
scintillation, all field types, separate particle truth, correct panel/hit joins,
one PDE decision per arrival and equality of tracked counts with ideal Npe.
There are no unjoined rows and no Geant4 exception in the final log.

Complete arrival and path distributions were retained. Sensor 0 reaches
10597.2783 ns and 1994173.997 mm ancestral path; sensor 1 reaches 450.4789 ns
and 82825.7574 mm. Four/six arrivals later than `t0+100 ns` remain in the PE
counts. The finite electronics gate affects measured ADC/TDC, not transport
survival or raw optical truth.

clas12root completed with `response_mode=2`, all additional jitter zero and
calibration disabled because this technical sample is too small. It does not
establish a timing resolution. The actual analysis sources and their private
test copy have matching recorded SHA256.

- [Final HIPO](output/mine_20261104_094329_23027.hipo)
- [Run log](output/mine_20261104_094329_23027.log)
- [Verification record](validation/geant4_pipeline_6gm2PJvX/final_2events/VERIFIED.txt)
- [All raw arrivals](validation/geant4_pipeline_6gm2PJvX/final_2events/arrivals.csv)
- [Complete empirical CDFs](validation/geant4_pipeline_6gm2PJvX/final_2events/arrival-summary.json)
- [Analysis summary](validation/geant4_pipeline_6gm2PJvX/final_2events/analysis-results/geant4_final/summary.json)

## Observed performance

The standalone 2000-photon straight/bend runs took approximately 0.4/0.7 s wall
time; the 372795-step replay took 0.554 s. The final two-event GEMC run reported
47.4949 s total CPU time, including 47.2427 s for events. Its transparent green
fiber model permits rare kilometer-scale skew paths; the first event alone
had taken approximately 43 s in a preceding check. These are local diagnostic
observations, not a long-production throughput estimate.

The real tabulated event reported 0.865796 s total and 0.645640 s for events.
clas12root took 0.801659 s wall time on the final two-event file with the compiled
macro reused; fresh compilation/analysis had taken 7.999 s on the one-event file.

## Physical assumptions and remaining scope

Scintillator Birks is 0.126 mm/MeV, yield 8000 photons/MeV and decay 2.5 ns.
Bulk attenuation is intensity survival exp(-actual ray distance/1000 mm).
Fiber indices are the reference nondispersive PS/PMMA values 1.59/1.49; the
single-cladding radial thickness is 0.020 mm on 1 mm outer diameter.

The active 0.34 mm WLS blue absorption length is derived near 430 nm at 200 ppm
and explicitly used as an approximate 425 nm proxy. Independent NOvA blue-light
data imply approximately 0.30 mm. This is not the manufacturer's axial 3.5 m
attenuation length. Sources and the derivation are in
[GEANT4_WLS_SOURCES.md](GEANT4_WLS_SOURCES.md).

The phase-1 model has monochromatic 425→476 nm proxies, unity WLS yield and
6.9 ns decay. Green reabsorption and microscopic core/cladding absorption are
disabled; the separate optional effective fiber path-loss proxy also defaults
to disabled. Thus the observed long tails are properties of this declared ideal
model, not a validated broadband Y-11 prediction. Full spectral reabsorption,
dispersion and measured losses need additional data. The 20.5 mm ideal bundle
readouts are not a verified physical coupling to a 3×3 mm commercial SiPM.

Geant4's real polarization does not reproduce a general complex Jones state.
Other layouts, YS-6 and four-sensor semi_parallel remain later extensions.
Only Geant4 10.7.4 was compiled and tested here. Numerical failures abort with
diagnostics; they are not recorded as physical attenuation.

## Modified and added files

- Geometry/material/tracking: `source/hitprocess/mine/mine_optical_geometry_model.{h,cc}`,
  `mine_geant4_optics.{h,cc}`, `mine_geant4_tracking.{h,cc}`, `mine_optical_event.h`.
- Shared response: `mine_parameters.{h,cc}`, `mine_response.{h,cc}`,
  `mine_hitprocess.cc`, `mine_hipo_schema.h`; canonical bank definitions in
  `geometry_source/mine/mine.pl` and `experiments/mine/mine__bank.txt`.
- Framework: `source/gemc.cc`, `source/physics/PhysicsList.cc`,
  `source/sensitivity/sensitiveDetector.cc`,
  `source/src/{MDetectorConstruction,MEventAction,MSteppingAction}.cc`,
  `source/output/{hipoSchemas,hipo_output}.{h,cc}`, `source/SConstruct`,
  `source/CMakeLists.txt`, and the validation-artifact entry in `.gitignore`.
- Launcher/tests: `scripts/{run_mine.sh,mine_launch_card.py,test_mine_launch.py,
  test_mine_optics.sh,compare_mine_fiber_benchmarks.py,test_mine_fiber_comparison.py}`;
  the preserved external-analysis patch in `scripts/patches/`.
- C++ validation: `source/hitprocess/mine/tests/{optical_geometry_model_test.cc,
  optical_geometry_reference_test.cc,geant4_response_test.cc,
  geant4_fiber_benchmark.cc,geant4_scintillation_smoke.cc,hipo_optical_check.cc,
  hipo_output_test.cc,export_reference_fiber_benchmark.C,export_reference_long_launch.cc}`.
- Configuration/docs: `experiments/mine/{geant4_optics.conf,
  geant4_optics.conf.example,geant4_geometry_only.conf,GEANT4_OPTICS.md,
  GEANT4_WLS_SOURCES.md,VALIDATION_20261103.md,VALIDATION_20261104.md,README.md,PHYSICS.md}`.
- External analysis, only where required: `simulation_time_resolution/src/TimingAnalysis.cpp`,
  its `README.md` and `tests/make_synthetic_hipo.cc`. Old tabulated HIPO files
  remain accepted; merging Geant4 and tabulated responses is rejected.

`MINE::dgt` now has 58 fields, adding `responseMode` and `Nscintillation` while
preserving existing field definitions. `MINE::optical` (4600/3) has 11 fields
with time, wavelength, full ancestral path, panel/sensor/track identity, PDE
decision, hit join and entry medium. No optical absorption is added to particle
Edep or t0. The configuration fingerprint is 2401140501197208 for the active
file; it identifies configuration contents, not a source-code revision.
