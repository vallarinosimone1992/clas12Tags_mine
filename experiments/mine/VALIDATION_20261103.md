# MINE explicit optics: validation checkpoint, 2026-11-03

This historical checkpoint is superseded by the completed
[2026-11-04 validation](VALIDATION_20261104.md).

The explicit implementation and tests are being validated against the actual
container. The existing tabulated response has passed its complete particle to
analysis regression. Explicit optical collection and timing results remain
pending the final geometry/propagation corrections described below.

## Configuration and preserved inputs

- Container: `mine-gemc-vis`, Geant4 10.7.4, GCC 11.5.0, CLHEP 2.4.7.1,
  HIPO 4.5.0. Installed compiler/include/library details:
  [environment.txt](validation/optics_6pxeDgb3/environment.txt).
- Initial full application build: completed successfully against installed
  headers. [Build log](validation/pre-optical-build.p7wJ4KgW/build.log);
  the previous `source/gemc` executable is preserved in that same directory.
  Later geometry fixes require the subsequent rebuild and runtime checks.
- Phase 1: `corner_fan`, 15 round Y-11 single-cladding fibers, two sensors.
  Active effective blue WLS length: 0.34 mm. Its near-430 nm derivation and
  approximate use at monochromatic 425 nm are documented in
  [GEANT4_WLS_SOURCES.md](GEANT4_WLS_SOURCES.md).
- Scintillator: Birks 0.126 mm/MeV, 8000 photons/MeV, exponential 2.5 ns,
  bulk survival `exp(-actual optical path / 1000 mm)`.
- No reference-MC source, existing optical table, simulation data or previous
  analysis result was overwritten. The working tree already contained modified
  and untracked MINE files before this phase; these were preserved.

## Results actually observed

| Check | Result and quantitative evidence |
| --- | --- |
| Portable routing and coordinate model | PASS: pitch 94.1121432583 mm, inset 10.2689885377 mm, readout width 20.5012151476 mm; central/outer route lengths 1393.67558530/1868.11220358 mm |
| Current reference-MC geometry parity | PASS: maximum centerline discrepancy 4.26326e-14 mm; maximum exact-arc versus 2 mm chord length difference 0.00245933 mm |
| Launcher preflight | 8 tests pass, including Geant4 startup archive without an optical table and incompatible-mode rejection |
| ECDF/binomial comparator | 6 tests pass, including confidence intervals, ties, paired covariance and refusal to overwrite |
| Legacy effective SiPM response, actual container | PASS: PE mean 49.9563, variance 50.3824 |
| Existing tabulated response, actual container | PASS: dual sensors, independent full-tail times, WLS bookkeeping, compatible-table checks |
| Explicit arrival response, actual container | PASS: ideal arrival counts/times retained; one Bernoulli PDE; panel association; 200 photons at PDE 0.25 over 5000 trials give mean 49.8884 and variance 37.6879 against 50 and 37.5 |
| Updated clas12root synthetic regression | PASS: 12000 synthetic events, 10800 deposits; corrected mean-time sigma 0.435700 ns against Gaussian expectation 0.430116 ns; no-hit retention, jitter quadrature, selection and overwrite checks passed |
| Real tabulated particle → HIPO → clas12root | PASS: one 1 GeV/c muon, seed 571203; one deposition and timing valid on both sensors; exact table hash 3103114980582929; analysis `response_mode=1` |
| Optical HIPO reader with synthetic IO fixtures | PASS: joined arrivals, empty tabulated optical bank and optical-only unjoined arrivals accepted; inconsistent Npe/PDE counts rejected. This checks IO, not optical transport |

Preserved actual tabulated artifacts:
[tabulated.hipo](validation/tabulated_bWuIXs7t/tabulated.hipo),
[analysis summary](validation/tabulated_bWuIXs7t/analysis-results/tabulated/summary.json),
[completion marker](validation/tabulated_bWuIXs7t/analysis-results/tabulated/COMPLETE.txt).
The actual synthetic-regression artifacts and source hash are in
`validation/optics_HUbd2cpD`. Deterministic train/test assignment depends on the
canonical file path, so small width differences between private copies are
expected; the input noise model and test tolerances remain fixed.

## Observed performance

The portable/analysis validation recorded 0.965 s for launcher tests, 0.714 s for
portable geometry compilation and 13.776 s for the complete synthetic clas12root
regression. GEMC reported 0.865796 s total and 0.645640 s for its single tabulated
event. These timings are local diagnostic observations. Explicit-transport
throughput has not yet been established at this checkpoint.

## Pending optical checks

The initial standalone benchmark compiled after correcting a `pi` name collision
with CLHEP. Execution then exposed sensor-boundary geometry and Geant4-10.7.4
velocity bookkeeping issues. Final corrected runs must establish:

1. Straight-fiber endpoint probabilities, physical paths and times.
2. R=100 mm, 90-degree bend versus equal-length straight reference: endpoint
   losses, complete ECDFs and timing/path tails, with statistical uncertainties.
3. `corner_fan` panel with no sampled overlaps, collection at both sensors and
   physical arrival times.
4. Explicit optical particle → HIPO → clas12root validation using the active
   configuration and a final rebuilt executable.

No uncompleted check is counted as passed. Full CDFs are required for timing;
the reference effective maps' conditional means are not used as replacements.
Standard real Geant4 polarization and the MC's complex Jones transport are not
assumed numerically equivalent.

## Output schema and code groups

`MINE::dgt` now has 58 fields. Existing fields remain; `responseMode` distinguishes
legacy effective 0, tabulated 1 and Geant4 2, while `Nscintillation` records the
actual generated scintillation count in mode 2 and uses -1 in tabulated mode.
`MINE::optical` (4600/3) contains 11 fields: panel, sensor, track, parent, time ns,
wavelength nm, path length mm, detected, pdeApplied, hitn and entryRole. The
complete schema and sentinel meanings are in [GEANT4_OPTICS.md](GEANT4_OPTICS.md).

The main implementation groups are:

- Exact geometry/material model and event tracking:
  `source/hitprocess/mine/mine_{optical_geometry_model,geant4_optics,geant4_tracking}.{h,cc}`
  and `mine_optical_event.h`.
- Response reuse: MINE parameters, hit process and response files; canonical bank
  definitions in `geometry_source/mine/mine.pl` and generated MINE schema/bank.
- Framework integration: `source/gemc.cc`, detector/event/stepping actions,
  sensitive detector and optical physics configuration; SConstruct/CMake inputs.
- HIPO: `source/output/hipoSchemas.{h,cc}` and `hipo_output.{h,cc}`.
- Host selection and tests: `scripts/run_mine.sh`, `mine_launch_card.py`,
  `test_mine_launch.py`, `test_mine_optics.sh`, fiber comparator and its tests,
  plus isolated geometry/response/benchmark/HIPO checks under the MINE tests.
- External analysis: narrowly scoped `src/TimingAnalysis.cpp`, README and
  synthetic HIPO fixture changes in `simulation_time_resolution`; the applied
  patch is preserved in `scripts/patches/simulation_time_resolution-geant4.patch`.

Reproducible commands, including a binary backup before explicit GEMC rebuild,
are in [GEANT4_OPTICS.md](GEANT4_OPTICS.md). The short runner never rebuilds GEMC
implicitly and caps integration runs at two events per mode.
