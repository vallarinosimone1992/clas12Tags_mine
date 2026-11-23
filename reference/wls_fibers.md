# Fibre WLS: Y-11, YS-6 e YS-4 con perdita ordinaria comune

**Storico, superato il 22 novembre 2026:** i 4 m costanti descritti qui non sono
piu attivi. Il modello corrente e in [wls_spectral_loss.md](wls_spectral_loss.md).
I risultati e le ipotesi del 20 novembre rimangono conservati per confronto.

Implementazione e verifica: 20 novembre 2026. Modalita attiva: **trasporto
esplicito Geant4** (`--transport geant4`). Il default `tabulated` del launcher
e il vecchio modello efficace non sono modificati. La baseline dei 46000 eventi
gia archiviata rimane quella originaria, senza assorbimento ordinario della fibra.

## Fonti, input dell'utente e campo di applicazione

- **Documentato:** Kuraray, meeting 13 novembre 2026, PDF p.7 (spettri e limiti
  di attenuazione), p.8 (attenuazione a dose zero); copia originale e SHA256 in
  [manufacturers/kuraray_20261113](manufacturers/kuraray_20261113/assessment.md).
  Le curve relative sono misurate in soluzione di monomero di stirene; i limiti
  tabellari p.7 sono per fibra 1 mm Non-S multi-cladding. Non sono certificati
  per il lotto MINE. P.8 distingue single/multi: Y-11 single circa 3.8 m,
  YS-6 single circa 3.8 m, YS-4 single circa 3.4 m a dose zero, **letture visive
  approssimate** senza errori sperimentali forniti.
- **Documentato:** Herzkamp 2016, Fig.4.14 e Tab.4.4: Y-11 e coefficiente
  assoluto di picco 0.0147 ppm^-1 mm^-1. Restano i limiti della fonte originale
  descritti in [data Y-11](../experiments/mine/data/optics/y11/README.md).
- **Input utente:** adottare proprieta comuni dove non sono documentate
  differenze, perdita efficace condivisa da core e cladding, mantenere Y-11,
  YS-6 e predisporre YS-4 suggerita dal produttore; verifica su fibra diritta.
- **Ipotesi:** medesimi polimeri, stessa scala di assorbimento di picco per i tre
  coloranti, concentrazione effettiva 200 ppm anche per le YS, QY=1, convenzione
  photon/nm, indici costanti, cladding singolo. **La concentrazione e il coefficiente
  assoluto delle YS non sono misure del produttore.**

## Valori implementati

| Proprieta | Y-11 | YS-6 | YS-4 | Provenienza |
|---|---:|---:|---:|---|
| Picco di assorbimento nominale [nm] | 430 | 414 | 420 | Kuraray p.7/tabella p.6 |
| Picco di emissione nominale [nm] | 476 | 462 | 470 | Kuraray |
| Decay WLS [ns] | 6.9 | 1.3 | 1.4 | Kuraray, costante intrinseca nominale |
| Concentrazione [ppm] | 200 | 200 effettivi | 200 effettivi | Y-11 di riferimento; trasferimento alle YS |
| Coefficiente di picco [ppm^-1 mm^-1] | .0147 | .0147 | .0147 | Herzkamp Y-11; trasferimento alle YS |
| Perdita ordinaria lungo il raggio [mm] | 4000 | 4000 | 4000 | Ipotesi comune, vincolata all'attenuazione globale |
| Indice core PS / cladding PMMA | 1.59 / 1.49 | identici | identici | Valori nominali Kuraray, senza dispersione |
| Quantum yield WLS | 1 | 1 | 1 | Ipotesi mantenuta |

La lunghezza ordinaria di **4 m** e un valore arrotondato scelto per ottenere
una lunghezza assiale vicina a 3.8 m, non una misura della sola matrice PS o PMMA.
Nell'approssimazione di guida meridionale, mu=cos(theta) e in [1.49/1.59,1]:
la componente assiale di un singolo raggio e L_z=mu*L_path, cioe 3.75-4.00 m
prima di considerare riassorbimenti WLS e selezione dei modi. Il benchmark
qui sotto verifica la combinazione completa.

**Geometria invariata:** fibra circolare single-cladding, diametro esterno 1 mm,
PMMA radiale 0.02*D=20 um, diametro core 0.96 mm. Il 4% della presentazione
si riferisce al doppio cladding (2% PMMA + 2% FP); non viene aggiunto un secondo
strato. Non si scambia una percentuale radiale con una frazione di cammino ottico.

## Leggi e distinzione delle perdite

    alpha_WLS(lambda) = .0147 * C_eff * Arel_profile(lambda)  [mm^-1]
    L_WLS(lambda) = 1/alpha_WLS(lambda)                    [mm]
    P_ordinary(s) = exp(-s/4000 mm)
    1/L_core_total = 1/L_core_independent + 1/L_common
    1/L_clad_total = 1/L_clad_independent + 1/L_common

Gli ultimi due termini indipendenti sono disabilitati per default (valore 0
nelle chiavi significa processo disabilitato, non assorbimento immediato).
`effectiveFiberAttenuationMm=4000` aggiunge la stessa probabilita di perdita per
unita di **cammino reale** in entrambi i materiali. WLSABSLENGTH rimane un processo
separato: ogni riassorbimento produce un figlio con spettro proprio e ritardo
esponenziale della fibra scelta. Nessuna attenuazione o ritardo efficace viene
applicato nuovamente nell'hit process.

La curva di *transmission loss* in dB/km e conservata nei CSV per YS-4/YS-6:
L_z[m]=4342.944819/A[dB/km]. **Non viene installata una seconda volta come ABSLENGTH.**
Il modello comune e costante in lambda: NON riproduce le bande della transmission
loss spettrale, specialmente nel rosso dove i dati mostrano lunghezze maggiori.
La verifica dell'attenuazione globale non valida quindi la trasmissione a ogni
lambda. La separazione tra perdite di matrice, deboli code WLS non risolte,
scattering e superfici resta indeterminata. L'approssimazione puo influire
sull'evoluzione dello spettro e sulle code temporali: non e una caratterizzazione
microscopica completa.

## Digitalizzazione e convenzioni

Y-11 conserva i vertici vettoriali Herzkamp per non alterare arbitrariamente
il profilo precedente. YS-4 e YS-6 usano gli spettri raster incorporati nella p.7
Kuraray, colori originali, calibrazione affine degli assi e campionamento ogni
2 nm, con interpolazione lineare attraverso tratti tratteggiati/occlusi.
Pixel circa 0.66 nm e 0.0052 di ordinata relativa; i decimali del CSV non sono
precisione sperimentale. Le ordinate di assorbimento disegnate sotto zero
vengono cambiate di segno e normalizzate. Scarti di circa 2 nm tra il massimo
digitalizzato e quello nominale tabellare sono compatibili con la lettura raster.
L'emissione sotto la soglia di due pixel viene troncata a zero entro 2 nm
 dall'ultimo punto risolto; nessuna lunga coda debole viene inventata.

Si assume dN/dlambda (photon/nm): dN/dE e proporzionale a S(lambda)*lambda^2.
L'alternativa `wlsEmissionConvention=power_per_nm` aggiunge un fattore lambda.
Dati e grafici: [kuraray_wls](../experiments/mine/data/optics/kuraray_wls/README.md).
L'hash ottico comprende config e tabelle compilate, distinguendo i profili.

## Benchmark Geant4 su fibra diritta

Geant4 10.7.4, compilatore registrato nei log. 6000 fotoni per profilo,
seed 20261120. Fibra diritta 1 mm single-cladding in aria; posizioni uniformi
nell'area del core, spettro intero del profilo, direzioni isotrope **condizionate
al cono guidato meridionale verso +z**, polarizzazione casuale trasversa.
La selezione aumenta la statistica del test; NON introduce un trapping imposto
nella produzione GEMC. Non include tutti i modi skew del protocollo Kuraray.

I medesimi helper di produzione installano RINDEX/ABSLENGTH/WLSABSLENGTH/
WLSCOMPONENT. Riassorbimenti, riflessione e rifrazione alle interfacce sono tracciati;
nessun taglio temporale. Si conta il primo attraversamento del core o cladding
della singola linea fotone/figli a ogni piano virtuale: 0.1,0.25,0.5,1,1.5,2,3,4,5,6 m.
Ogni linea conta una volta per piano, anche dopo WLS.

Fit esponenziale della componente lunga fra 1 e 6 m, likelihood multinomiale
condizionata ai sopravvissuti a 1 m. I conteggi ai vari piani sono correlati:
non si usa un fit con bin indipendenti. Errori dalla curvatura della likelihood,
condizionati al modello; non includono sistematiche dei materiali/digitalizzazione.

| Profilo | L_z simulata [m] | Errore statistico [m] | Lettura grafico Kuraray [m] |
|---|---:|---:|---:|
| Y-11 | 3.920 | 0.086 | circa 3.8 |
| YS-6 | 3.964 | 0.120 | circa 3.8 |
| YS-4 | 4.072 | 0.110 | circa 3.4 |

Accordo con le letture grafiche entro il 20%, tolleranza di lavoro dichiarata
per questo modello semplificato, **non incertezza sperimentale attribuita a Kuraray**.
YS-4 resta circa il 20% sopra il punto a dose zero: il modello non riproduce
esattamente le differenze di attenuazione fra coloranti. I limiti nominali
>3.5 m Y-11, >3.0 m YS-4/YS-6 sono rispettati anche sottraendo 1.96 errori
statistici, nel benchmark definito. Cambiano costruzione/modi rispetto al test
tabellare multi-clad, quindi questo confronto e un controllo di plausibilita.

Controllo indipendente a 600 nm (seed 20261121): senza riassorbimento WLS,
la sopravvivenza nativa concorda con l'integrale analitico sul cono guidato
<exp[-(z-z_source)/(L_path*mu)]>. Test: scarto inferiore a 6 errori binomiali
piu 0.002 assoluti a tutti i piani. Anche i test nativi in lastra verificano
Beer-Lambert, spettro/trasformazione energetica e decadimento dei tre profili.

Sono stati compilati GEMC e i test; un evento di muone da 1 GeV per ciascun
profilo ha prodotto un HIPO completo e leggibile, con arrivi su entrambi i
sensori. Verificati conteggi, associazione ai depositi, PDE ideale unica e ID
fibra nel bank `MINE::dgt`: 0 Y-11, 1 YS-6, 2 YS-4.

Risultati compatti: [JSON](validation/wls_20261120/attenuation_summary.json),
[PNG](validation/wls_20261120/straight_fiber_attenuation.png), conteggi CSV e hash
nella stessa cartella. Log completi e HIPO di controllo in
`build/validation/wls_20261120/`; il sottoinsieme finale per il benchmark e `benchmark/`.
Tentativi preliminari sono conservati separatamente; `verified/` contiene gli
smoke HIPO finali ma un precedente test statistico dello spettro fallito.
Il test e stato corretto usando la varianza dello spettro e il numero effettivo
di riemissioni: la soglia fissa di 1.5 nm era inappropriata con circa 130 fotoni.
L'esecuzione completa finale e `benchmark-run.log`.

**Numerical validation is not experimental validation.** Questa verifica non
certifica le proprieta del lotto MINE, il quantum yield o la risoluzione temporale
reale. Rimangono attive le altre approssimazioni, incluso SiPM ideale.

## Configurazione e riproduzione

Profili: `experiments/mine/geant4_optics_y11.conf`, `geant4_optics_ys6.conf`,
`geant4_optics_ys4.conf`, tutti `model=spectral_wls`. Il vecchio nome
`spectral_y11` resta un alias compatibile per il modello spettrale;
`fiberModel=Y-11|YS-6|YS-4` seleziona le curve (alias Y11/YS6/YS4 ammessi).
Se omessi, `wlsDecayNs` e `wlsWavelengthNm` vengono dal profilo; se presenti,
sono override espliciti. Gli altri config spettrali esistenti, incluso quello
di default `geant4_optics.conf`, attivano ora la perdita comune di 4000 mm.
`geant4_optics_monochromatic.conf` e `geant4_geometry_only.conf` rimangono storici.

```sh
# Dalla radice del repository; cambiare ys4 in ys6 o y11.
scripts/run_mine.sh --batch --transport geant4 \
  --optical-config experiments/mine/geant4_optics_ys4.conf -N=1000

# Ricompilare GEMC nel container dopo modifiche ai sorgenti.
docker exec mine-gemc-vis bash -lc \
  'cd /work/clas12Tags_mine; source scripts/docker_env.sh; cd source; scons -j2'

# Nuova directory automatica; non sovrascrive risultati precedenti.
scripts/test_mine_wls_fibers.sh --photons 6000

# Rigenerazione dei dati da una copia locale del documento originale.
python3 scripts/extract_mine_kuraray_wls.py \
  --pdf reference/manufacturers/kuraray_20261113/kuraray_psf_meeting_20261113.pdf \
  --output-dir /tmp/kuraray-wls-extracted
```

File principali: `mine_wls_spectrum.h`, `mine_kuraray_wls_data.h`,
`mine_optical_geometry_model.{h,cc}`, `mine_geant4_optics.{h,cc}`,
`mine_response.{h,cc}`, `mine_hitprocess.cc`, launcher e test. I tempi e gli
arrivi ottici registrati restano quelli completi Geant4.
