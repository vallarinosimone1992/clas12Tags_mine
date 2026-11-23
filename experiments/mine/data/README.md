# Dati di riferimento della risposta MINE

- SiPM Hamamatsu S13360-3075CS: [CSV PDE](s13360_3075cs_pde.csv), [PNG PDE](s13360_3075cs_pde.png). Fonte S07 nel [registro](../../../reference/sources.md): datasheet p.4, curva 75 µm a 25 °C e +3 V. Ancora nulla 270 nm imposta, traccia leggibile da circa 280 nm. Nel trasporto Geant4 attuale il SiPM è forzato ideale: la curva è riferimento, non PDE attiva del ramo.
- [Scintillatori](optics/scintillators/README.md), [Y-11](optics/y11/README.md), [rivestimenti](optics/coatings/README.md): CSV e provenance locali, con PNG accanto ai dati.

La [galleria centrale](../../../reference/plots/README.md) include tutti i dati e confronti fra materiali. I PNG sono 1800×1200 px, λ 250–900 nm e ordinate relative 0–1; la sola lunghezza di assorbimento usa un asse logaritmico in mm. Riproduzione dalla radice: `python3 scripts/plot_mine_references.py`.
