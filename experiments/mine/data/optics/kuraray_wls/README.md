# Kuraray WLS spectra and spectral transmission

Current explicit Geant4 model, 22 November 2026. Source: Kuraray meeting
13 November 2026 p.7; original PDF and SHA256 in
`reference/manufacturers/kuraray_20261113`. CSV raster extraction is unchanged.

- `ys4/ys6_absorption.csv`, `ys4/ys6_emission.csv`: normalized source spectra.
  `mine_kuraray_wls_data.h`: compiled copy. Blue absorption used through 480 nm;
  unresolved red WLS tail is constrained by transmission as documented below.
- `ys4/ys6_transmission.csv`: measured-curve digitization in dB/km.
  `mine_kuraray_transmission_data.h`: compiled copy generated from these CSVs.
- `y11_transmission_proxy.csv`: **substitute, not a Y11 measurement**; arithmetic
  mean of YS4/YS6 loss coefficients on their common measured support.
- `loss_components.csv`: exported from production helpers, with rates [mm^-1]
  for the identical ordinary core/cladding loss and separate WLS reabsorption.
- `provenance.json`: original source extraction; `transmission_provenance.json`:
  conversion, substitution, decomposition, domains, extension assumptions.

No source curve is installed twice. For lambda>=520 nm ordinary loss follows
Kuraray; below 520 nm it is held at its 520-nm value. In the measured blue tail,
WLS receives only the excess of source loss over that ordinary background.
Resolved blue WLS absorption <=480 nm stays unchanged; a logarithmic coefficient
bridge joins it to the first resolved transmission point. Equal core/cladding
loss is an effective assumption, not equality of PS/PMMA refractive indices.
The 4-m scalar is disabled. No global attenuation target is imposed.

At effective 200 ppm the two rates sum to the input monochromatic extinction
in the measured domain. At other concentrations only the WLS rate scales.
The decomposition is not unique experimental information: it assumes red
losses are ordinary and the blue excess is WLS, and treats the source as
unshifted near-axis transmission. Native tests verify that defined observable;
full-lineage transmission and integrated attenuation are separate diagnostics.

Reproduce source spectra with `scripts/extract_mine_kuraray_wls.py` and compiled
transmission arrays/proxy with `scripts/generate_mine_kuraray_transmission.py`.
Both generated headers must match the copies in `source/hitprocess/mine`.
Reproduce component export with `source/hitprocess/mine/tests/fiber_loss_test.cc`.
PNG gallery: `python3 scripts/plot_mine_references.py` (1800x1200, axes labelled).

Full physical assumptions, formulas, validation and remaining limitations:
[reference/wls_spectral_loss.md](../../../../../reference/wls_spectral_loss.md).
The [20 November scalar model](../../../../../reference/wls_fibers.md) is historical.
