# Y-11 blue absorption: source audit

This audit documents the historical monochromatic reference. Since 2026-11-04
the active Geant4 config uses spectral_y11, extending this peak normalization
with the vector curves of Herzkamp Fig.4.14. See data/optics/y11/README.md.

Research checked 2026-11-03 for the phase-1 Y-11(200), 1 mm diameter model.
The microscopic length used for blue WLS absorption is distinct from the
catalogue's attenuation length measured along a fibre after fluorescence.

## Quantitative near-peak input

[Herzkamp, RWTH Aachen dissertation (2016), section 4.3, equations 4.3.1-2
and table 4.4, printed pages 48-49](https://publications.rwth-aachen.de/record/673051/files/673051.pdf#page=50)
reports Kuraray transmission measurements for Y11 dye: 18.2 ppm, 10 mm optical
thickness, 430 nm illumination and transmission 6.89%. The table gives
`k(430 nm) = 0.0147 ppm^-1 mm^-1`, with

```
I/I0 = exp[-k(wavelength) * concentration_ppm * ray_length_mm]
L_WLS = 1 / (k * concentration_ppm)
L_WLS(430 nm, 200 ppm) = 1 / (0.0147 * 200) = 0.340 mm
```

The concentration scaling is the stated Beer-Lambert model. The measurement
is reported in the dissertation as manufacturer data, rather than a new
measurement on the particular MINE fibre. Its caption says "10 cm" while the
table's `d[mm]=10`, its coefficient and its transmission are mutually consistent
with **10 mm**. That inconsistency prevents treating the inferred number as a
manufacturer-certified specification.

For a monochromatic 425 nm model, **0.34 mm is a near-430 nm proxy**, not a
measurement at exactly 425 nm. No verified absolute 425 nm coefficient or
certified uncertainty was found. Keep the length configurable and report it
with generated results.

## Independent blue-light cross-check

[The NOvA Technical Design Report (2007), section 11.4, printed page 11-4](https://digital.library.unt.edu/ark:/67531/metadc898387/m1/316/)
gives WLS absorption lengths of 0.4 mm at 150 ppm and 0.2 mm at 300 ppm for
scintillation light in the Kuraray K27/Y11 core. Inverse-concentration scaling
therefore gives approximately **0.30 mm at 200 ppm**. This is a useful check of
the submillimetre scale, not an independent monochromatic 425 nm calibration.
The difference between 0.30 and 0.34 mm should not be converted into an invented
statistical uncertainty.

The 1 mm fibre diameter determines the available chord through the doped core;
it does not itself define the microscopic absorption length. For example, a
0.96 mm straight chord and `L_WLS=0.34017 mm` give `1-exp(-0.96/L)=0.9405` WLS
absorption probability, before refraction, fluorescence yield or trapping.
Geant4 must compute the actual path and boundaries.

## What the catalogue and spectral paper establish

[Kuraray's manufacturer page](https://methacrylate.kuraray.com/en/products/psf/wavelength-shiftingfiber/)
lists the Y-11(200) absorption/emission peaks as 430/476 nm and an attenuation
length above 3.5 m. Its attenuation measurement fits light versus distance along
the fibre between 100 and 300 cm. **3.5 m must not become the blue microscopic
`WLSABSLENGTH`.** Current listed reference data are for 1 mm multiclad Non-S
fibres; they are representative rather than guaranteed values.

[Pahlka et al., arXiv:1911.03790, section 4 and figure 4](https://arxiv.org/html/1911.03790v1#S4)
separates polystyrene absorption from K27 absorption/re-emission and uses
Kuraray-based spectra at 200 ppm. The plot and available text do not provide
a verified tabulated absolute coefficient at 425 nm. Its adopted quantum yield
0.7 and time constant 11.8 ns are not substituted for the explicitly selected
phase-1 ideal hypotheses, quantum yield 1 and decay 6.9 ns.

The NOvA report also describes strong reabsorption below 490 nm within less than
0.5 m. Consequently, the phase-1 monochromatic model with green reabsorption
disabled does not establish realistic long-distance Y11 attenuation or its
complete arrival-time distribution. Full spectral reabsorption needs additional
validated input; a catalogue axial exponential is not an automatic replacement.
