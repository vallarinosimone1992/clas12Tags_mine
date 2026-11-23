# Grafici delle reference MINE

PNG **1800 × 1200 pixel**, 180 dpi, identica area del grafico e asse λ **250–900 nm**.
Asse verticale **0–1 lineare** per intensità relative, coefficienti relativi, riflettanza, trasmittanza e PDE.
Gli indici di rifrazione hanno limiti verticali comuni **1–1.65**, lineari e adimensionali.
La lunghezza di assorbimento ha unità diverse: **0.1–1000 mm, logaritmico**; stessa scala orizzontale.

Tutti i CSV spettrali presenti sono rappresentati. Non sono inventate curve per i parametri scalari.
I valori di riflettanza/trasmittanza/PDE non sono rinormalizzati; gli spettri sono già relativi nei CSV.
Non viene convertita la trasmittanza FNAL in lunghezza di assorbimento assoluta.
Linee fra i punti: interpolazione della fonte/modello. Tratteggi dei rivestimenti: estensione a estremi costanti.

I PNG individuali e i confronti di famiglia sono copiati identici accanto ai CSV; questa è la galleria centrale.
[manifest.json](manifest.json) registra input/hash, scale, copie e formula della derivazione Y-11.

Riprodurre dalla radice del repository (numpy, matplotlib e Pillow):

```sh
python3 scripts/plot_mine_references.py
```

Per una prova in un'altra cartella senza aggiornare le copie presso i dati:

```sh
python3 scripts/plot_mine_references.py --output-dir /tmp/mine-reference-plots --no-adjacent-copies
```

Fonti e limiti: [catalogo](../sources.md), [decisioni](../decisions.md).

## Spettro di emissione — scintillatore estruso FNAL

![Spettro di emissione — scintillatore estruso FNAL](fnal_emission.png)

## Spettro di emissione — scintillatore colato EJ-200

![Spettro di emissione — scintillatore colato EJ-200](ej200_emission.png)

## Trasmittanza relativa — scintillatore estruso FNAL

![Trasmittanza relativa — scintillatore estruso FNAL](fnal_relative_transmission.png)

## Spettri e trasmittanza — scintillatori FNAL ed EJ-200

![Spettri e trasmittanza — scintillatori FNAL ed EJ-200](spectra_and_transmission.png)

## Assorbimento relativo — fibra WLS Kuraray Y-11

![Assorbimento relativo — fibra WLS Kuraray Y-11](y11_absorption.png)

## Spettro di emissione — fibra WLS Kuraray Y-11

![Spettro di emissione — fibra WLS Kuraray Y-11](y11_emission.png)

## Lunghezza di assorbimento WLS — Y-11 a 200 ppm

![Lunghezza di assorbimento WLS — Y-11 a 200 ppm](y11_wls_absorption_length_200ppm.png)

## Riflettanza — TiO₂ coestruso FNAL / NICADD

![Riflettanza — TiO₂ coestruso FNAL / NICADD](fnal_tio2_reflectance.png)

## Riflettanza relativa — Tyvek 150 µm

![Riflettanza relativa — Tyvek 150 µm](tyvek_reflectance.png)

## Riflettanza relativa — Mylar alluminato 10 µm

![Riflettanza relativa — Mylar alluminato 10 µm](aluminized_mylar_reflectance.png)

## Riflettanza di riferimento — vernice BC-621

![Riflettanza di riferimento — vernice BC-621](bc621_reflectance.png)

## Riflettanza spettrale — rivestimenti FNAL, Tyvek e Mylar

![Riflettanza spettrale — rivestimenti FNAL, Tyvek e Mylar](coating_spectra.png)

## PDE — SiPM Hamamatsu S13360-3075CS

![PDE — SiPM Hamamatsu S13360-3075CS](s13360_3075cs_pde.png)

## Spettri di emissione — FNAL, EJ-200 e Kuraray Y-11

![Spettri di emissione — FNAL, EJ-200 e Kuraray Y-11](emission_spectra_comparison.png)

## Indice di rifrazione — aria ambiente

![Indice di rifrazione — aria ambiente](air_refractive_index.png)

## Indice di rifrazione — gel ottico Thorlabs G608N3

![Indice di rifrazione — gel ottico Thorlabs G608N3](gel_refractive_index.png)

## Indice di rifrazione — colla ottica Eljen EJ-500

![Indice di rifrazione — colla ottica Eljen EJ-500](glue_refractive_index.png)

## Indice di rifrazione — aria, gel e colla ottica

![Indice di rifrazione — aria, gel e colla ottica](groove_refractive_indices.png)

## WLS absorption spectra: Kuraray Y-11, YS-6 and YS-4

![WLS absorption spectra: Kuraray Y-11, YS-6 and YS-4](kuraray_wls_absorption.png)

## WLS emission spectra: Kuraray Y-11, YS-6 and YS-4

![WLS emission spectra: Kuraray Y-11, YS-6 and YS-4](kuraray_wls_emission.png)

## Kuraray spectral loss: YS-4, YS-6 and Y-11 proxy

![Kuraray spectral loss: YS-4, YS-6 and Y-11 proxy](kuraray_wls_transmission.png)

## Spectral loss budget: Kuraray Y-11 substitute

![Spectral loss budget: Kuraray Y-11 substitute](kuraray_y11_loss_budget.png)

## Spectral loss budget: Kuraray YS-6

![Spectral loss budget: Kuraray YS-6](kuraray_ys6_loss_budget.png)

## Spectral loss budget: Kuraray YS-4

![Spectral loss budget: Kuraray YS-4](kuraray_ys4_loss_budget.png)


## Mini-panel geometry

[mini_panel_spiral.png](mini_panel_spiral.png) is a separate 1800×1200-pixel
geometry diagram (not a spectral curve): native XY centerlines, 1×2-mm gel
groove and tangent 30° air/Mylar tent. Axes are in millimetres. Regenerate from
the portable model CSV with `scripts/plot_mine_mini_panel.py`; geometry/provenance
and numerical tolerances are documented in [mini_panel_spiral.md](../mini_panel_spiral.md).
