# Versioned Y-11 spectral reference

These files contain vertices extracted from the **vector paths**, not pixels,
of Matthias Herzkamp's 2016 RWTH Aachen dissertation, printed p.48,
Fig.4.14 (PDF page 50). The dotted Y11 absorption and emission curves are used.
The dissertation attributes the spectra to Kuraray. They are representative
curves, not measurements on the particular MINE fiber batch.

- Source: https://publications.rwth-aachen.de/record/673051/files/673051.pdf
- Download mirror: https://www.fz-juelich.de/++api++/de/jcns/jcns-2/downloads/dipdoc/doktorarbeit_herzkamp_pdf/@@download/file
- Attribution: Matthias Herzkamp, *Simulation and Optimization of a Position
  Sensitive Scintillation Detector with Wavelength Shifting Fibers for Thermal
  Neutrons*, RWTH Aachen / Forschungszentrum Juelich, 2016. The publication
  states CC BY 4.0. Curves have been extracted and converted as described below.
- `provenance.json` records the PDF SHA256, axis calibration and limitations.
- `y11_absorption.csv` contains relative **absorption coefficient**, not length.
- `y11_emission.csv` contains the relative emission ordinate versus wavelength.
- `mine_y11_spectral_data.h` is the generated compile-time copy. Its identical
  copy in `source/hitprocess/mine/` is used by the application; no external
  response table, PDF, SVG or ROOT installation is required at run time.

## Absolute normalization and concentration

The absorption ordinate below zero in the figure has been sign-reversed and
normalized to 1 at 430 nm. Piecewise-linear interpolation is in wavelength.
Herzkamp Table 4.4 gives k(430 nm)=0.0147 ppm^-1 mm^-1. For concentration C:

    coefficient(lambda) = 0.0147 * C * relative_absorption(lambda)  [mm^-1]
    L_WLS(lambda) = 1 / coefficient(lambda)                       [mm]

At 200 ppm L(430)=0.340136 mm, L(425)=0.387812 mm, L(480)=10.4246 mm.
This is a **dye absorption** coefficient, not a trapping fraction or axial
attenuation length. The table's 10 mm thickness, 18.2 ppm and 6.89% transmission
agree with the coefficient. Its caption instead says 10 cm; that inconsistency
remains a normalization systematic, not a resolved manufacturer certificate.
The source's historical decay constant is NOT imported: MINE keeps 6.9 ns.

## Support and uncertainty

There are 24 absorption and 30 emission vertices. Their precision in the files
records the extracted vector coordinates, **not experimental precision**.
Absorption is resolved over approximately 350-490 nm and emission over 430-640
nm. The graphic does not constrain a weak red absorption tail. Outside the
provided support, or where the curve is zero, the dye absorption coefficient
is set to zero: this is a declared working assumption, not measured infinite
transparency. In Geant4 a finite 1e15 mm represents that zero rate. The short
blue-wavelength support edge is also a hard support limit; the blue scintillation
proxy must be inside the measured interval. There is no fitted/extrapolated tail.

Thus reabsorption at 476-490 nm is now modeled; reabsorption above the plotted
zero and wavelength-dependent nonradiative polymer absorption are not calibrated.
Do not add the doped-polystyrene curve of Pahlka Fig.4 as a second WLS coefficient:
ordinary absorption and dye absorption must remain separate. That figure's
metre-scale linear axis does not resolve blue sub-mm absorption accurately.
A useful independent spectral-transport reference is Pahlka et al., 2019,
https://arxiv.org/abs/1911.03790, section 4 and figures 4-5.

## Emission convention and Geant4 conversion

The source labels the ordinate only as relative emission; photon counts versus
radiant power are not specified. Default `wlsEmissionConvention=photon_per_nm`
interprets it as a photon-number density per nm. With E=hc/lambda:

    dN/dE = (dN/dlambda) * lambda^2 / (hc)

The energy-density Jacobian is applied before building WLSCOMPONENT. The common
normalization is arbitrary. `power_per_nm` instead interprets the plot as radiant
power per nm and applies an additional division by photon energy, hence a
lambda^3 factor. This alternative is available for the spectral systematic.
Neither convention is asserted as a measured fact about the source.

The native property grid has 0.25 nm spacing plus every source vertex and guard
points at the support edges; it is strictly ascending in energy, covers 1-6 eV,
and uses linear Geant4 property interpolation (no spline overshoot).
Each actual WLS absorption/re-emission uses the exponential 6.9 ns delay;
Geant4 restricts secondary energy to the absorbed photon's energy or below.
QY remains exactly one, with no Poisson multiplicity. No trapping or transport
delay is added by the hit process. The original scintillation source remains
monochromatic at 425 nm; this change models the **fiber**, not the scintillator
emission spectrum or refractive-index dispersion.

## Reproducing the data

With Poppler installed, run from the repository root:

```sh
python3 scripts/extract_mine_y11_spectra.py --pdf /path/herzkamp2016.pdf \
    --output-dir /tmp/y11-extracted
```

Compare the generated CSV/header/provenance with this folder. Update both header
copies when deliberately changing the reference. The optical configuration hash
includes the compiled spectral vertices, version and interpolation convention,
so spectral runs cannot be silently mixed with earlier monochromatic runs.

## PNG di confronto

- [Coefficiente relativo di assorbimento](y11_absorption.png).
- [Spettro di emissione](y11_emission.png).
- [Lunghezza di assorbimento WLS a 200 ppm](y11_wls_absorption_length_200ppm.png): derivata con la formula sopra, asse verticale logaritmico 0.1–1000 mm; valori a coefficiente nullo e code non vincolate non sono disegnati come misure.

Formato comune 1800×1200 px e λ 250–900 nm; per le ordinate relative scala 0–1. Galleria e rigenerazione: [reference/plots](../../../../../reference/plots/README.md).
