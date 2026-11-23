# Mini-panel: EJ-200, gel and SiPM physical inputs

Review dated 22 November 2026. This note supports the `mini_panel_spiral`
implementation. It distinguishes manufacturer properties from working
transport assumptions; numerical validation is not experimental validation.

## EJ-200 material preset

The user selected cast plastic of EJ-200 type. The current official Eljen
[product table](https://eljentechnology.com/products/plastic-scintillators/ej-200-ej-204-ej-208-ej-212)
and [August 2023 datasheet, p.2](https://eljentechnology.com/images/products/data_sheets/EJ-200_EJ-204_EJ-208_EJ-212.pdf)
give the following nominal inputs:

| Property | Nominal value | Application |
|---|---:|---|
| Photon yield | 10000 photons/MeV electron | Scintillation normalization; Birks is applied separately |
| Emission maximum | 425 nm | Use the full existing digitized EJ-200 spectrum, not a monochromatic replacement |
| Rise time | 0.9 ns | Native finite-rise scintillation parameter |
| Decay | 2.1 ns | Native fast-component time constant |
| Base | PVT | `G4_PLASTIC_SC_VINYLTOLUENE` composition proxy |
| Density | 1.023 g/cm3 | Override the base-material density |
| Refractive index | 1.58 | Constant-index approximation; wavelength dispersion unavailable |
| Technical light attenuation | 380 cm | Reference observable, not an intrinsic bulk absorption table |

The old `geant4_optics_ej200.conf` changed only the spectrum while retaining
extruded-plastic yield 8000 and decay 2.5 ns. It was not a complete EJ-200
preset. The mini-panel preset should use the nominal values above. The
existing `data/optics/scintillators/ej200.csv` is a source digitization; its
photon/nm versus radiant-power convention and red-tail truncation at 500 nm
remain explicitly documented in that dataset's README.

`birksMmPerMeV=0.126` remains an inherited/user-accepted working parameter;
it is not supplied by the Eljen table and is not experimentally established
for this particular cast sample. No plastic self-absorption/re-emission or
measured Rayleigh model is introduced by choosing the EJ-200 emission profile.

### Bulk absorption must remain labelled provisional

Eljen's older single-product sheet explains that its roughly 4-m attenuation
was measured in cast strips, with a representative size 2 x 20 x 300 cm:
[manufacturer-authored sheet, university archive](https://courses.washington.edu/phys433/equipment/EJ200data-sheet.pdf).
This is an axial detector-level light-transport observable. Reflections,
spectrum and accepted directions can contribute; it cannot uniquely determine
microscopic `ABSLENGTH(lambda)` along the actual ray path.

Accordingly, retaining `bulkAbsorptionLengthMm=1000` is a provisional explicit
microscopic model assumption inherited from the existing simulation, not a
measured EJ-200 property. In particular, it must not be justified by calling
Eljen's 380 cm an intrinsic absorption length. A future slab-transmission
measurement or an appropriately matched strip benchmark is needed to constrain
this input. It can materially affect collection; its uncertainty is distinct
from the Y-11 fiber-loss model.

## Native finite-rise law and its interpretation

The installed toolkit is Geant4 10.7.4. Its actual headers expose
`G4OpticalParameters::SetScintFiniteRiseTime(bool)` and the legacy material key
`FASTSCINTILLATIONRISETIME`. The production physics list enables this flag only
when `scintillationRiseNs>0`; historical configs default to zero. In the enhanced
11.x convention the corresponding material key is `SCINTILLATIONRISETIME1`.
[Official optical-process guide](https://geant4.web.cern.ch/documentation/dev/bfad_html/ForApplicationDevelopers/TrackingAndPhysics/physicsProcess.html).

The installed `G4Scintillation.hh` defines its finite-rise time density as:

    f(t) = (tr+td)/td^2 * exp(-t/td) * [1-exp(-t/tr)],  t>=0
    te = tr*td/(tr+td)
    F(t) = 1 - [td exp(-t/td)-te exp(-t/te)]/(td-te)
    mean(t) = td+te
    variance(t) = td^2+te^2

For the nominal 0.9/2.1-ns parameters, `te=0.63 ns` and the mean emission delay
is 2.73 ns. It is not implemented as the sum of 0.9-ns and 2.1-ns exponentials.
Importing the named datasheet rise time directly as the native rise parameter
is a working kinetic convention: the datasheet does not provide a complete
multi-component emission-time fit or define a unique microscopic kinetic model.

`tests/geant4_scintillation_rise_test.cc` tests that native density with a
simple 8-mm PVT slab. It samples actual primary-muon scintillation, reconstructs
the within-step transit from each photon's birth position and compares delay
CDF/mean. Optical children are discarded before propagation, so this diagnostic
does not claim a collection or detector-time prediction. A separate mini-builder
integration check must verify that the configured rise property reaches the
actual mini-panel material table.

### Native generation test result

The standalone test was compiled and run against Geant4 10.7.4. Five primary
muons per case, seed 20261122; emitted optical children are inspected and killed
before propagation:

| Rise / decay [ns] | Photons | Mean delay [ns] | Expected [ns] | CDF KS |
|---|---:|---:|---:|---:|
| 0.9 / 2.1 | 68530 | 2.73601 | 2.73000 | 0.00269806 |
| 0 / 2.1 | 66537 | 2.10430 | 2.10000 | 0.00202794 |

Both pass the explicit mean and CDF checks. Logs, executable and probe configs
are in `build/validation/mini_physics_20261122/`; source is
`source/hitprocess/mine/tests/geant4_scintillation_rise_test.cc`. The zero-rise
case confirms the historical exponential convention remains available. These
are generation-only results, not a mini-panel timing-resolution validation.

## G608N3 in grooves and the 1-mm sensor gap

The user selected the current gel reference in both places, with a 1-mm
sensor gap. Thorlabs' manufacturer catalog page provides the Cauchy fit:

    n(lambda_nm) = 1.4338 + 10520/lambda_nm^2

Published points include n(402)=1.4995, n(589.3)=1.4646, n(980)=1.4462 and
n(1550)=1.4378; specific gravity is 1.06 at 25 C. Stable manufacturer source:
[Thorlabs catalog page, April 2024](https://www.thorlabs.de/catalogpages/Obsolete/2024/ADASC1.pdf).
It also bounds absorption below 0.003% per micrometre at 450-750 nm.

The existing `mine_groove_materials.h` implementation already provides this
index and native group velocity; the same material should fill the sensor gap.
Ignoring gel absorption remains the user's accepted approximation. The stated
bound alone would allow up to roughly 3% loss across 1 mm, so zero absorption
must not be presented as a manufacturer measurement. Below 402 nm the index fit
is extrapolated, and the quoted absorption bound does not cover the 425-nm peak.
The existing PDMS composition is only a charged-particle transport proxy for
an unspecified commercial formulation.

## Optional simplified SiPM response

The current model uses the digitized Hamamatsu S13360-3075CS PDE curve at the
working conditions 25 C and +3 V, if selected. Source and original digitization
are retained as S07 in [sources.md](sources.md):
[Hamamatsu datasheet](https://hep.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s13360_series_kapd1052e.pdf).
The user requests ideal detection by default, with an optional nonideal model.

Existing implementation assumptions in `mine_response.cc`:

- Each resolved optical arrival receives one Bernoulli PDE decision, without
  resampling its tracked population or adding effective transport delays.
- The PDE already includes fill factor; it is not multiplied by fill factor again.
- The simplified response has 1600 randomly addressed cells, gate occupancy
  without recovery, one-generation prompt cross-talk and dark pulses. Current
  parameters are 7% cross-talk and 500 kcps/device; afterpulsing remains disabled.
- Gain stays normalized to one; no intrinsic/electronic timing jitter is added.
- Ideal mode means PDE=1, no pixel losses or noise. Nonideal mode must not be
  silently forced back to ideal by the explicit-Geant4 parameter loader.

These are not a spatial microcell model, recharge simulation or full detector
calibration. Photons rejected by PDE should remain in optical truth with
`detected=0`; first-photon timing for a nonideal analysis must select detected
photons rather than all arrivals. `Npe` represents primary detected PE, whereas
`Nfired` additionally reflects occupancy/noise/cross-talk conventions.

## Tight Non-S bends

The user confirmed Y-11 Non-S. The 30-mm inner radius is the specified measured
mini geometry. It is tighter than the nominal mechanical recommendation for
1-mm Non-S fiber (about 100-mm radius) discussed in the
[Kuraray meeting assessment](manufacturers/kuraray_20261113/assessment.md).
Native Geant4 geometric bending loss is already tracked. Stress, damage,
microscopic roughness and microbending caused by this tight routing remain
uncalibrated; no arbitrary extra bending multiplier should be added on top.
