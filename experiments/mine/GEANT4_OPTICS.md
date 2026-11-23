> **Aggiornamento groove e output MC (12 novembre 2026):** i preset espliciti correnti usano groove aperti 2×2 mm sulla faccia z_MC=0, centro fibra z_MC=1 mm e riempimento `grooveMaterial=gel|air|glue` (default gel). Le descrizioni sotto del contatto diretto plastico/cladding e di z_MC=.5 mm rimangono riferimenti storici senza groove. Nuova banca `MC::WLS` per ogni assorbimento e quattro campi fibra in `MINE::optical` (ora 15 campi). [Dettagli, fonti e selezione](../../reference/grooves.md).

# Explicit Geant4 optical transport: phase 1

The explicit optical branch is selected separately from the existing tabulated
response. Its first supported panel is `corner_fan`, Y-11 round single-cladding
fiber and two ideal readout planes. The table remains the launcher default.
Other routing geometries, YS-6 and the four-sensor semi-parallel layout are not
implemented in this branch.

```sh
# Existing response, including its complete conditional timing distributions.
scripts/run_mine.sh --batch --transport tabulated --geometry corner_fan -N=2 -RANDOM=23456

# Geometry/interface diagnostic, WITHOUT WLS absorption or re-emission.
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan \
  --optical-config experiments/mine/geant4_geometry_only.conf -N=1 -RANDOM=23456

# To select a different effective absorption hypothesis:
cp experiments/mine/geant4_optics.conf.example /tmp/mine-optical-model.conf
# Replace wlsAbsorptionLengthMm=REQUIRED in that private configuration.
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan \
  --optical-config /tmp/mine-optical-model.conf -N=1 -RANDOM=23456
```

The Geant4 branch reads `MINE_GEANT4_CONFIG`, populated by the launcher from its
private startup snapshot. It does not open or validate the optical MC table.
Unknown/duplicated configuration keys and incompatible models fail explicitly.
The active `geant4_optics.conf` now selects **spectral_y11**, using digitized
Herzkamp/Kuraray Y-11 absorption and emission curves at configurable dye
concentration. See [data/optics/y11/README.md](data/optics/y11/README.md) for
source provenance, normalization and the unresolved-tail limitations.

`geant4_optics_monochromatic.conf` preserves the earlier constant 0.34 mm
near-430 nm proxy and monochromatic 476 nm emission. Its source audit is
[GEANT4_WLS_SOURCES.md](GEANT4_WLS_SOURCES.md). The `.example` template and
geometry-only diagnostic retain this historical monochromatic model; the
latter sets its WLS length to zero. Sections below describing the old blue
step/green transparency refer to that optional model, not spectral_y11.

## Geometry and coordinate convention

`mine_optical_geometry_model.cc` adapts the updated reference MC formulas
`ResolveCornerFanSizing` and `BuildDiagonalCornerFanPaths` from
`MC_MINE_LightTrapping/src/AlternativeGeometries.cpp`, read on 2026-11-02.
The reference project is not modified or needed at GEMC runtime.

Each noncentral fiber is a straight cylinder, an exact circular toroidal sector,
and a second tangent cylinder. Core and cladding each form one Boolean solid;
separate physical segments do not introduce optical end faces at joins.
A numerical Boolean weld extends each straight by `1e-4 mm` into its torus.
The envelope deviation is bounded by `weld²/[2(R-r)]`, below Geant4 surface
tolerance for the supported reference radii; it is not a physical gap or a
separate overlapping volume. The builder rejects a radius for which that
bound exceeds the toolkit tolerance.

The cladding physical volume contains the concentric core daughter. For the
nominal 1 mm outer diameter and `claddingFraction=0.02`, radial cladding
thickness is 0.020 mm and core radius is 0.480 mm. No skin or reflective coating
is applied to scintillator–cladding or cladding–core interfaces. Their smooth
dielectric boundaries remain available for Fresnel reflection/refraction and
total internal reflection.

The MC uses `0 <= z <= thickness`; GEMC's Box is centered at zero. Coordinates
are related by `z_local = z_MC - thickness/2`. Thus `zMcMm=0.5` places the
reference fibers at local `z=-4.5 mm` in the 10 mm panel, including their full
outer radius. This differs from interpreting the MC number directly as GEMC
local z. The corner fan has no vertical layer staggering in the current MC.
Optional `fiberZOffsetsMm=a,b,...` supplies one constant offset per fiber and
is checked against both the panel and the common readout height. Planes and
fiber positions remain in panel coordinates; placement of the entire panel is
handled by its Geant4 transformation.

Fibers terminate at distinct positions on two common finite planes, with
oblique cuts normal to each readout plane. Auto-fit retains the MC endpoint
separation and full elliptical outer-cladding footprints. For the 15-fiber
configuration in the supplied files:

| Quantity | Value |
|---|---:|
| Effective virtual-vertex pitch | 94.1121432583 mm |
| Effective readout inset | 10.2689885377 mm |
| Common readout width | 20.5012151476 mm |
| Common readout height | 1.5 mm |
| Central fiber length | 1393.67558530 mm |
| Outermost fiber length | 1868.11220358 mm |
| Fillet radius | 101 mm |

These are ideal MC readouts, not a claim that a 20.5 mm bundle fits the active
3×3 mm area of one S13360-3075CS. Physical bundling or coupling optics remains
an extension. A thin 0.01 mm absorbing numerical readout slab starts behind
each exact plane and is clipped analytically to the panel mother. The nominal
configuration uses a directly placed `G4Box`: its physical z interval is
[-5,-3.75] mm, height 1.25 mm and center -4.375 mm. This is the portion of the
nominal 1.5 mm MC plane that intersects the scintillator; the front-plane XY
position and all fiber footprints are unchanged. All eight box corners are
checked against the panel. Configurations requiring additional XY clipping use
an analytically clipped `G4ExtrudedSolid` polygon, without a Boolean intersection
of a tiny slab and the whole panel. It retains plastic
composition/density for particle transport, but has no scintillation emission
properties. Its optical skin has zero reflection and zero Geant4 detection
efficiency. The panel response records **every** sensor entrance, including
cladding arrivals and direct plastic illumination through gaps, and sends each
arrival to the common PDE/electronics path. This follows an ideal photosensor
entrance definition. The reference MC's panel counter instead accepts only
core-to-readout arrivals and records the other entrances as `ReadoutMiss`.
Those two counter definitions must be distinguished in comparisons. The
`MINE::optical.entryRole` field retains the entrance medium, allowing the
MC-compatible core subset (`entryRole=1`) to be selected. The isolated-fiber
benchmark below deliberately retains the MC core-only definition.

The exterior coating uses **border surfaces to the containing environment**.
`coatingModel=specular|diffuse` selects mirror/Lambertian angular laws;
`coatingMaterial=aluminized_mylar|fnal_tio2|tyvek` selects source-specific R(lambda).
The default specular profile now uses the Yang2021 Al-Mylar spectrum; the diffuse
reference uses the untuned FNAL/Mu2e TiO2 table, and a separate Tyvek profile is
provided. Yang values are relative to BaSO4; `coatingReferenceAlbedo=1` is an
explicit ideal-reference approximation. `coatingMaterial=constant` retains a
scalar `reflectivity` for historical/user hypotheses. Sources and limitations:
[data/optics/coatings/README.md](data/optics/coatings/README.md) and the central
[reference register](../../reference/README.md). Surviving reflected photons keep
wavelength and acquire no intrinsic fluorescence delay.
The main border is panel→environment.
Each cladding also has a border to that same environment: with the nominal
`z_MC=0.5 mm` and 0.5 mm outer radius, it touches the outside bottom face, and
Geant4 can navigate directly cladding→world there. The same outer wrap must
cover that contact. These borders never coat cladding→scintillator or
core→cladding interfaces. A panel skin is avoided because it would also coat
internal fiber boundaries. The original uncovered contact was identified by
the fatal `NoRINDEX` diagnostic at z=-5 mm; the final two-event test passes with
that exterior contact correctly covered. No imposed trapping
probability or guided-cone cut is used. `checkOverlaps=1` requests Geant4 overlap
checks of cladding, core and readout volumes and rejects detected overlaps.
Sampled checks complement the analytic/reference geometry checks; they are not
a mathematical proof of arbitrary user configurations.

## Material data, proxies and attenuation

The scintillator parameters are Birks `0.126 mm/MeV`, light yield
`8000 photons/MeV` of quenched visible energy, and one exponential scintillation
decay with `2.5 ns` time constant. The explicit branch generates and propagates
Geant4 photons. Its packet response does not generate another photon population
or append another scintillation decay. Standard Geant4 with `RESOLUTIONSCALE=1`
uses its normal fluctuation prescription for large step means, whereas the
tabulated helper uses Poisson sampling. Equal mean yield does not imply identical
step-by-step population statistics.

Every optical material owns its material-properties table independently.
Composition, density, thermodynamic state, mean excitation energy and Birks
constant are copied explicitly from the corresponding reference material.
This matters in Geant4 10.7.4: its density-derived material constructor shares
the base table pointer, and replacing that table in a derived readout could
delete the scintillator's emission table. Independent composition copies remove
that ownership coupling. The generation-only standalone smoke check now observes
Edep=1.80991 MeV, Evis=1.76956 MeV, expected population 14156.5 and 14095 actual
scintillation photons for one deterministic 1 GeV/c muon. That diagnostic counts
births and explicitly discards optical children afterward; it makes no collection
or propagation prediction. Its preserved log is
`validation/fiber_Nov3_XqivKzTB/scintillation-smoke-composition.log`.

Nondispersive working indices are 1.58 for plastic, 1.59 for PS core and 1.49
for PMMA cladding. Propagation time follows the Geant4 material/group-velocity
properties. The former effective plastic speed of 170 mm/ns and effective fiber
speed are not appended. With a constant plastic index, `c/1.58` is approximately
189.7421 mm/ns; ray angles and actual paths remain explicit.

The installed Geant4 10.7.4 required a boundary-relocation velocity workaround,
verified with individual step traces. After a reflection, the zero-length
`StepTooSmall` relocation can copy the temporary incident material's velocity
into the next finite flight. A core ray was consequently propagated with the
cladding velocity. `SynchronizeOpticalVelocity` now reads the actual destination
material's `GROUPVEL` after that relocation and synchronizes the track and
post-step velocity before its next flight. The workaround is guarded specifically
for version 10.7.4 and is shared by GEMC and the isolated benchmark. It adds no
delay, path correction or effective-speed constant. Rechecking the physical
identity `t=integral(n ds/c)` gives maximum residuals 1.77636e-15 ns for 2000
straight photons and 8.88178e-16 ns for 2000 bent-fiber photons. Other toolkit
versions require rerunning that check before asserting the same behavior.

`bulkAbsorptionLengthMm=1000` defines **intensity survival**
`P(s)=exp(-s/1000 mm)` along the actual optical photon distance traveled in
scintillator. It is a 1/e survival length, not an amplitude decay length, a
distance to the nearest fiber, an axial distance, or an attenuation of the
particle's deposited energy. This agrees with the actual-path weighting
convention of the MC response exporter. An attenuation length of zero disables
that loss rather than creating an infinitely strong absorber.

The current `OpticalFiberProfile.h` provides Y-11 nominal/work-profile values
476 nm emission peak, 6.9 ns decay, 1.59/1.49 indices and radial cladding
thickness 2% of the outer diameter. Its catalogue attenuation value 3500 mm is
explicitly a **working path-loss proxy**: the catalogue measurement is an axial
fit on 1 mm multiclad Non-S samples. It is not a measured microscopic
absorption length for these single-cladding photons. The older panel MC default
of 0.030 mm radial cladding is therefore not silently substituted for the
updated nominal isolated-fiber profile.

The default microscopic `coreAbsorptionLengthMm` and
`claddingAbsorptionLengthMm` are zero, meaning disabled. The independent
`effectiveFiberAttenuationMm` is also zero. If deliberately enabled, the latter
adds `exp(-actual core-plus-cladding ray distance / length)` as an **effective**
loss proxy, implemented together with explicitly configured microscopic loss
through additive optical depths. No catalogue conversion happens automatically.
Do not also apply an exporter/table attenuation to these tracked photons.

The reference panel uses effective `WLS.AbsorptionProbability=1` upon a fiber
interception. It supplies no wavelength-dependent absorption spectrum, no
microscopic WLS absorption length and no measured quantum yield. That surface
probability does not determine a finite volumetric absorption length.
Consequently the MC alone provides no nonzero `wlsAbsorptionLengthMm`.
The active explicit configuration now supplies the separately researched
0.34 mm near-430 nm proxy, as documented in GEANT4_WLS_SOURCES.md.

`model=monochromatic_effective` is the explicitly named initial approximation:

- Unshifted emission uses the existing 425 nm proxy; WLS emission uses the
  updated Y-11 476 nm proxy. Neither is a measured full spectrum.
- Geant4 vectors represent each proxy by a numerical triangular delta with
  relative energy half-width `1e-8`, solely because interpolation needs distinct
  energy points. This width is not a physical spectral broadening.
- The user-selected WLS absorption length is constant in the blue band. A
  sharp effective edge halfway in energy between the two proxies disables
  reabsorption at the emitted wavelength. This is an effective two-color model,
  not a reconstructed manufacturer absorption curve.
- Absorption/re-emission occurs inside the core, with isotropic Geant4 WLS
  emission and one 6.9 ns exponential delay. Quantum yield is explicitly fixed
  at one in phase 1. Other values are rejected instead of misusing
  `WLSMEANNUMBERPHOTONS`, which would introduce a Poisson multiplicity.
- `wlsAbsorptionLengthMm=0` disables WLS; it is provided for geometry diagnostics
  and must not be presented as a Y-11 collection prediction.

The optical properties only support this initial spectral model. A broadband
Cherenkov prediction requires appropriate measured optical spectra rather than
treating the numerical RINDEX support interval as experimental data.

## Timing, SiPM response and output identity

Tracked arrivals carry panel/sensor association, arrival time, wavelength,
track/parent identity and the full ancestral path across WLS generations.
HIPO stores these rows in the new `MINE::optical` bank (group 4600, item 3):
`panel/I`, `sensor/I`, `track/I`, `parent/I`, `time/D`, `wavelength/D`,
`pathLength/D`, `detected/I`, `pdeApplied/I`, `hitn/I`, `entryRole/I`. Units are ns,
nm and mm. Entrance roles are panel=0, core=1, cladding=2, sensor=3, unknown=-1.
`hitn=-1` means no associated depositing MHit, which can occur in optical-only
benchmarks; it does not mean the photon was lost. `detected=-1` marks a PDE
decision not yet applied, while 0/1 records rejection/detection afterward.
Arrival times already include scintillation emission, every transported path
and any WLS decay. The response adds no effective velocity, trapping factor,
additional WLS delay, scintillation delay or second attenuation.

PDE is evaluated once per arrival in the existing SiPM response, with the
arrival wavelength. The optical surfaces apply no PDE. Phase 1 explicitly uses
`ideal_SiPM=1` and zero additional timing jitter; the common
response path still supports the established saturation/noise/waveform and
digitization machinery. The electronics gate remains an explicit measurement
gate, not a photon-transport time cut: late arrivals are preserved even when
they do not contribute to gated ADC/TDC.

Particle energy-deposition truth is kept separate from optical absorption.
Optical steps cannot create a new particle hit or redefine its `t0`. Panel
association is retained through the core/cladding/readout hierarchy.
`t0` remains the earliest particle energy deposition for the panel.

The existing MINE bank retains its previous field IDs. Added fields are
`responseMode` (`0` legacy, `1` tabulated, `2` explicit Geant4) and
`Nscintillation` (actual generated scintillation-photon count, `-1` when
unavailable in the other branches). `opticalTable=0` in Geant4 mode;
`opticalHash` fingerprints its explicit configuration. An analysis that requires
`opticalTable=1` must accept `responseMode=2` separately and must not merge
tabulated and explicit optical responses as if they were identical.

## Polarization and reference comparisons

The reference MC propagates a complex Jones vector, retaining relative s/p
phase through total internal reflection and later rotations of the incidence
plane. Standard Geant4 optical photons carry a real transverse polarization
vector. Standard polarized Fresnel boundary handling therefore does not supply
the same general complex elliptic state history as that Jones transport.
Agreement of initial random linear ensembles or of a straight guiding-cone
calculation is not proof of equivalent multiply reflected bend transport.
Efficiency and complete arrival-time distributions must be compared directly;
no numerical equivalence is asserted here.

The MC effective fiber maps provide composed efficiencies and conditional
**mean** delays, not complete temporal distributions. They are not substituted
for a CDF when evaluating time resolution. This branch uses actual tracked
arrival distributions; the tabulated branch continues to sample its original
complete timing kernels.

## Checks executed and reproducible commands

The portable checks do not need Docker or Geant4:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Isource/hitprocess/mine \
  source/hitprocess/mine/tests/optical_geometry_model_test.cc \
  source/hitprocess/mine/mine_optical_geometry_model.cc \
  -o /tmp/mine-optical-geometry-test
/tmp/mine-optical-geometry-test experiments/mine/geant4_geometry_only.conf \
  experiments/mine/geant4_optics.conf.example
```

Observed result: PASS; both diagonals, odd/central and even fiber counts,
analytic tangency and endpoint planes, z transformation/offset validation,
and refusal of a missing WLS hypothesis were checked. The diagnostic configuration
loads successfully and the template's `REQUIRED` placeholder is rejected.

The read-only parity check additionally compiles the current reference
`AlternativeGeometries.cpp` and links its existing portable transport library:

```sh
c++ -std=c++17 -O2 -Isource/hitprocess/mine \
  -I../MC_MINE_LightTrapping/include \
  source/hitprocess/mine/tests/optical_geometry_reference_test.cc \
  source/hitprocess/mine/mine_optical_geometry_model.cc \
  ../MC_MINE_LightTrapping/src/AlternativeGeometries.cpp \
  ../MC_MINE_LightTrapping/build/libfiber_optical_transport.a \
  -o /tmp/mine-optical-reference-test
/tmp/mine-optical-reference-test
```

Observed result: PASS for 1, 7, 14 and 15 fibers and both diagonals. Maximum
centerline-point discrepancy was `4.26326e-14 mm`. The maximum analytic-arc
versus 2 mm MC chord length difference was `0.00245933 mm`.

An isolated Geant4 test is supplied for compilation inside the configured
container. It launches already emitted 476 nm photons isotropically in the core
with random transverse linear polarization, and explicitly terminates outer
side escapes to match the reference isolated-fiber experiment. It applies no
WLS, attenuation, PDE, imposed trapping or scintillation delay:

```sh
cd source/hitprocess/mine
g++ $(geant4-config --cflags) -std=c++17 -O2 -I. \
  tests/geant4_fiber_benchmark.cc mine_geant4_optics.cc \
  mine_optical_geometry_model.cc -o /tmp/mine-g4-fiber-benchmark \
  $(geant4-config --libs) -lG4ptl -lG4tasking
# Unique output paths: the benchmark refuses to overwrite existing files.
/tmp/mine-g4-fiber-benchmark straight 4000 23456 /tmp/mine-straight-new.csv 100 90
/tmp/mine-g4-fiber-benchmark bend     4000 23456 /tmp/mine-bend-new.csv     100 90
```

The straight case has the same total centerline length and source position as
the bend. CSV rows retain sensor, physical path length, full arrival time,
wavelength, and separate core/cladding path lengths. Assertions check complete
photon accounting, sampled Geant4 overlaps and `t = integral(n ds/c)`.
Compare full empirical arrival-time distributions, including tails, alongside
the probabilities at both ends and path distributions.

```sh
python3 scripts/compare_mine_fiber_benchmarks.py \
  --straight /tmp/mine-straight-new.csv --bend /tmp/mine-bend-new.csv \
  --generated 4000 --paired-launches --output /tmp/mine-fiber-comparison-new.json
python3 scripts/test_mine_fiber_comparison.py
```

The comparator preserves every unique value of the empirical time/path CDFs,
means and q10/q50/q90/q99, and reports empirical two-sample KS distances.
Collection probabilities have binomial standard errors and central
Clopper–Pearson intervals, including nonzero upper uncertainty when no photon
arrives. `--confidence` changes the interval coverage (default 68.268949%).
Ratios/reductions remain unclamped; a value above unity can represent a
statistical or geometric enhancement. `--paired-launches` asserts identical
source launches/event IDs and enables a covariance-aware delta-method ratio
error; this is not an exact ratio confidence interval. No independent-sample
KS p-value is invented for these correlated samples. JSON creation is exclusive
and refuses to overwrite an existing result. The six portable numerical/CLI
checks pass, including exact finite-binomial sums, confidence-interval endpoints,
ECDF ties, paired covariance and output preservation.

The Geant4 and MC bend benchmarks use the same **source distribution**: a disk
uniform in core area (`r=R_core*sqrt(U)`), isotropic directions and random
transverse linear polarization. The source lies at the input-lead midpoint,
25 mm from end 0; both leads are 50 mm. At R=100 mm and 90 degrees, both the
bend and its straight reference have centerline length 257.079632679 mm.
The MC's bend coordinates put the tangent junction at y=0 and the source at
y=-25 mm; the Geant4 test puts them at y=50 mm and y=25 mm. This is a rigid
translation, not a different source arclength.

The MC hashes a separate seed per photon; this Geant4 test uses one independent
source RNG stream. Equal numeric seeds across the two programs do not produce
identical individual launch states. Within the Geant4 straight/bend pair, an
equal seed does produce identical launches because transport draws use a
separate Geant4 RNG. Thus pair event IDs only within that pair. Compare MC/G4
ensembles statistically unless launch states are exported and explicitly replayed.
Use the MC bend scan's **equal-length straight reference** with attenuation,
microscopic absorption and initial WLS delay all disabled. The separately
configured default 200 mm straight MC has a central source and enabled 3500 mm
effective attenuation; its default output is not the matched reference for
this bend comparison. Real-vector/Jones differences described above remain.

Actual short straight/bend checks were run with Geant4 10.7.4 at R=100 mm,
90 degrees, seed 871209 and 2000 photons per geometry. The reference MC was run
with the same source distribution and optical constants, attenuation and WLS
delay disabled, and all outputs directed to a fresh GEMC validation directory.
Core-only arrival counts were exported from its ROOT tree with
`tests/export_reference_fiber_benchmark.C`; no reference-project file was
modified. Both programs reported zero cladding-end arrivals in this sample,
and the MC reported zero numerical censoring.

| Program and geometry | End 0 / 2000 | End 1 / 2000 | Mean end-1 time (ns) | Median (ns) | q99 (ns) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Geant4 straight | 132 | 131 | 1.89858 | 1.31839 | 12.6026 |
| Geant4 bend | 132 | 124 | 1.93096 | 1.31406 | 12.7773 |
| MC straight reference | 140 | 130 | 4.13836 | 1.30657 | 15.7946 |
| MC bend | 140 | 128 | 4.19172 | 1.30051 | 15.9594 |

The end-1 bend/straight ratio is 0.946565 ± 0.019654 for Geant4 and
0.984615 ± 0.010797 for the MC, using the paired delta-method errors. Geant4's
end-1 collection probabilities are 0.0655 ± 0.00553 (straight) and
0.0620 ± 0.00539 (bend), with binomial standard errors. Complete exact-binomial
intervals and empirical CDFs are preserved in
`validation/fiber_Nov3_XqivKzTB/geant4_summary.json` and `mc_summary.json`.

The cross-program end-1 empirical time/path KS distances are 0.08015 for straight
and 0.07535 for the bend; `geant4_vs_mc.json` preserves both distributions and
all quantiles. These small independent ensembles do not establish numerical
equivalence. Their means are particularly sensitive to rare skew rays: the MC
contains a 320.966 ns / 60.518 m straight arrival and a 322.210 ns bend arrival,
whereas the independently launched Geant4 sample reaches 20.534/20.599 ns.
Those tails are retained in the artifacts. Measured standalone wall times were
0.426 s (Geant4 straight), 0.647 s (Geant4 bend), and 3.175 s for the MC's two
2000-photon cases together. These include initialization on the present host
and are not long-production throughput measurements.

A deterministic replay additionally transported the MC's longest straight
output-end ray, photon 1105 at seed 871209. Its exact source position,
direction and initial linear polarization were reconstructed using the reference
library's actual seed and polarization routines and checked against the stored
ROOT launch. Geant4 reached end 1 after **372795 steps**, traveling
60517.679918143 mm in **320.965749811963 ns**, beyond both old framework step
thresholds and the 100 ns electronics gate. The matched MC values were
60517.679919040 mm and 320.965749817088 ns: differences of -8.97e-7 mm and
-5.13e-9 ns. Geant4's internal `integral(n ds/c)` residual was zero at reported
precision; wall time was 0.554 s. This verifies retention of one long physical
tail without asserting general Jones/Geant4 equivalence. The replay is a
standalone transport check; GEMC's explicit-mode stepping hook separately
bypasses its old generic optical step/time cuts.

Artifacts are `validation/fiber_Nov3_XqivKzTB/long_replay_comparison.json`,
`long_replay.csv`, `long_replay.log` and `mc_long_launch.csv`. To reconstruct a
new private replay from a single-case MC ROOT file:

```sh
c++ -std=c++17 -O2 -I../MC_MINE_LightTrapping/include $(root-config --cflags) \
  source/hitprocess/mine/tests/export_reference_long_launch.cc \
  ../MC_MINE_LightTrapping/build/libfiber_optical_transport.a \
  $(root-config --libs) -o /tmp/mine-export-long-launch
/tmp/mine-export-long-launch /path/to/matched-reference.root /tmp/new-long-launch.csv
MINE_FIBER_REPLAY=/tmp/new-long-launch.csv \
  /tmp/mine-g4-fiber-benchmark straight 1 871209 /tmp/new-long-arrival.csv 100 90
```

The exporter reads the MC file and library without modifying the reference
project. It refuses existing outputs and validates the reconstructed launch
before writing it. `MINE_FIBER_REPLAY` affects only this validation executable,
requires exactly one CSV row per requested photon, and does not change the
GEMC event generator.

The short validation runner collects the portable, container and optional
integration checks into a fresh preserved directory:

```sh
scripts/test_mine_optics.sh --portable --analysis-tests
scripts/test_mine_optics.sh --photons 2000
# Requires the updated GEMC binary already built; one event per response mode.
scripts/test_mine_optics.sh --photons 2000 --integration 1 \
  --optical-config experiments/mine/geant4_optics.conf
# Same active 0.34 mm blue-absorption hypothesis, without the validation runner:
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan -N=1
```

The runner does not rebuild GEMC implicitly. It compiles standalone checks into a
new directory and never replaces the application binary. The container HIPO4
installation is a static library, so its reader checks explicitly link `-llz4`.
Benchmark logs use Bash's `time`; `/usr/bin/time` is not installed in this image.

Before an explicit application rebuild, preserve the existing binary and build
log in a new directory inside the mounted repository:

```sh
docker exec mine-gemc-vis bash -lc '
  set -euo pipefail
  cd /work/clas12Tags_mine
  source scripts/docker_env.sh
  mkdir -p experiments/mine/validation
  backup=$(mktemp -d experiments/mine/validation/pre-optical-build.XXXXXXXX)
  if [[ -f source/gemc ]]; then cp -p source/gemc "$backup/gemc"; fi
  (cd source && scons -j4) > "$backup/build.log" 2>&1
  tail -n 8 "$backup/build.log"
  printf "Preserved binary and build log: %s\n" "$backup"
'
```

On 2026-11-03 the running container was directly queried: Geant4 **10.7.4**,
GCC **11.5.0**, CLHEP **2.4.7.1** and HIPO **4.5.0**. The actual flags, include
and library paths are captured in `validation/optics_6pxeDgb3/environment.txt`.
The application completed a full `scons -j4` build against those installed
headers; the successful log and preserved previous executable are in
`validation/pre-optical-build.p7wJ4KgW`. Geant4 10.7 uses the legacy
`FASTCOMPONENT`/`FASTTIMECONSTANT` scintillation keys and the available
`G4OpticalParameters` API. The guarded Geant4 11 property-name branch has not
been compiled in this environment.

Actual standalone response checks passed inside that container:

| Check | Observed result |
| --- | --- |
| Existing effective response | Poisson PE mean 49.9563, variance 50.3824 |
| Tabulated response | Both sensors, independent complete timing tails, WLS bookkeeping and compatibility rejection passed |
| Explicit resolved arrivals | Exact ideal arrivals/times and panel association passed; PDE applied once |
| Fixed 200 arrivals, PDE 0.25, 5000 repetitions | Mean 49.8884, variance 37.6879; binomial expectations 50 and 37.5 |

The applied clas12root analysis regression was run from a private copy of the
actual updated source, leaving the external project's existing build/results
untouched. Artifacts are in `validation/optics_HUbd2cpD`: eight launcher tests
passed in 0.965 s, portable geometry compilation took 0.714 s, and the complete
synthetic analysis regression took 13.776 s. Its 12000 synthetic events contain
10800 panel deposits; the corrected mean-time width was 0.435700 ns against
Gaussian expectation 0.430116 ns. This validates the analysis procedure rather
than predicting the detector's timing resolution.

A **real tabulated particle → HIPO → clas12root** regression also passed with
one deterministic 1 GeV/c muon, seed 571203, vertex `(0,20,-100) mm`.
`validation/tabulated_bWuIXs7t/tabulated.hipo` has one panel deposition,
58 correctly typed `MINE::dgt` fields, exact table hash `3103114980582929`,
matching `MC::True`, and valid timing on both sensors. The 11-field
`MINE::optical` schema is registered with zero arrival rows in this mode.
The analysis completed in `validation/tabulated_bWuIXs7t/analysis-results/tabulated`
with `response_mode=1`; calibration is correctly disabled for this tiny sample.
GEMC reported 0.865796 s total and 0.645640 s for events. These are application
reported timings for this check, not a throughput estimate for long runs.

The final **explicit optical particle → HIPO → clas12root** check passed on
2026-11-04 with two 1 GeV/c muons, seed 571203 and the active 0.34 mm model.
`MINE_OPTICAL_RESPONSE` deliberately pointed to a missing file. All 32 sampled
volume-overlap checks passed without warnings; the final log has no Geant4
exception. There were 25293 actual scintillation photons and 567/547 arrivals
and ideal detected PE. Both sensors have valid timing in both events. The
58-field response and 11-field optical bank preserve particle truth, one PDE
per arrival and correct panel/hit joins. Full raw time/path CDFs are retained;
4/6 sensor arrivals later than `t0+100 ns` remain in the PE counts. The longest
arrival is 10597.2783 ns with ancestral path 1994173.997 mm.

The completed analysis uses `response_mode=2`, zero additional jitter and no
calibration on this tiny sample. GEMC reported 47.4949 s total CPU time and
47.2427 s for events; clas12root took 0.801659 s wall time with the compiled macro
reused. Transparent green fiber transport can produce rare very long skew
paths, so these observations are not a production-throughput prediction or a
measurement of timing resolution. Full validated artifacts and the modified
file inventory are linked in [VALIDATION_20261104.md](VALIDATION_20261104.md).

The small straight/bend ensembles and exact long-ray replay above validate the
implemented optical model. They do not establish equivalence to complex Jones
transport or realistic broadband Y-11 reabsorption. The active model has unity
WLS yield, monochromatic proxies and green reabsorption disabled; calibrated
spectral data remain a physical extension.

No hidden transport-time cut or boundary-count loss is introduced. No long
production was run, and existing MC/GEMC data and result files were preserved.


## GUI: display memory and stopping runs

The explicit-optics GUI draws fiber centerlines sampled every 2 mm instead of
asking HepPolyhedron to tessellate the thin, large-radius Boolean unions. The
panel is wireframe and sensors remain physical display volumes. Tracking still
uses the exact core/cladding solids; this is a visualization change only.

All photons are transported and contribute to digitization. For display only,
the first 50 optical tracks per event are retained, with at most 2000 trajectory
points per displayed track (also bounded for non-optical display trajectories).
Displayed lines can therefore end before the photon's actual termination.
There is no photon time gate or change to photon yield. This prevents long skew
trajectories from exhausting GUI memory. These limits do not apply in batch.

From the repository root:

```sh
scripts/run_mine.sh --gui --transport geant4 --geometry corner_fan -N=1
scripts/stop_mine.sh --list
scripts/stop_mine.sh                 # stop all GEMC instances of this repository
scripts/stop_mine.sh --pid 1234      # PID inside the container
```

The launcher now labels each remote run and handles Ctrl+C by stopping only that
run, including under Rosetta. The stop script sends SIGTERM, waits five seconds,
and uses SIGKILL for remaining matching processes. It does not stop Docker,
X11, or VNC. Runs launched before this change lack the run label but can still
be listed/stopped by PID or with the default all-project selection. Prefer the
GUI Exit button to finalize the HIPO index; a forcibly interrupted HIPO may be
incomplete. A GUI with no explicit -N still starts with zero generated events:
set N. Events to 1 and click Run, or launch with -N=1.


## Spectral Y-11 fiber model (2026-11-04)

The default `geant4_optics.conf` now selects `model=spectral_y11` and
`wlsDyeConcentrationPpm=200`. The previous unchanged phase-1 configuration is
preserved as `geant4_optics_monochromatic.conf`. This changes only explicit
Geant4 transport; the tabulated MC mode still uses its selected table.

```sh
scripts/run_mine.sh --batch --transport geant4 -N=1
scripts/run_mine.sh --batch --transport geant4 \
  --optical-config experiments/mine/geant4_optics_monochromatic.conf -N=1
```

In spectral mode the scalar legacy `wlsAbsorptionLengthMm` must be omitted;
lengths come from the representative literature curve and concentration.
`wlsWavelengthNm` is not used to sample spectral emission. In the default
config this nominal legacy key has been retained only for compatibility; the
emission distribution comes from the versioned spectral dataset.

See `data/optics/y11/README.md` for the sources, digitization, normalization,
PDF SHA256, density-in-energy Jacobian, and declared support/ordinate assumptions.
The new `WLSABSLENGTH` permits reabsorption in the measured overlap region;
`WLSCOMPONENT` replaces the green monochromatic proxy. Each interaction is
handled by Geant4 with one secondary and the exponential 6.9 ns delay. The
SiPM receives the actual final wavelength and applies its existing spectral
PDE exactly once. No effective time or second attenuation is added.

The blue scintillation source, constant refractive indices, quantum yield 1,
ordinary bulk/core/cladding losses and electronics remain as configured. This
is an improved spectral reference, not a full optical calibration of MINE.
In particular the absence of a resolved long-wavelength absorption tail in
the graphic is not proof of zero red reabsorption. Both the mono and spectral
modes retain their distinct limitations; earlier "blue step/green disabled"
statements in this document describe the preserved monochromatic mode.

The spectral dataset and interpolation convention participate in opticalHash,
so analysis merge checks reject mixing old and new optical models. No HIPO
schema change is needed: MINE::optical already stores per-arrival wavelengths.

`tests/wls_spectrum_test.cc` checks the data, concentration scaling and energy
conversion. `tests/geant4_wls_spectrum_test.cc` uses the SAME property installer
as production, in a 0.5 mm matched-index slab: absorption vs Beer-Lambert,
emitted wavelength density, energy ordering and exponential delay. It tests
8000 primaries at each of 430/480/500 nm, separately for both models.
These checks are included in `scripts/test_mine_optics.sh`.
