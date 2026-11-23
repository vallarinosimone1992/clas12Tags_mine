# Groove aperti e identificazione delle fibre

Implementazione del 12 novembre 2026 su richiesta di Simone, con conferma esplicita della faccia di apertura.

## Geometria e interfacce

Groove quadrati **2×2 mm** seguono la stessa linea centrale delle fibre, con lati laterali normali al percorso nel piano XY. Le curve sono settori anulari esatti, non curve discretizzate. La fibra circolare da 1 mm è concentrica alla sezione quadrata. Scanalatura aperta sulla faccia `z_MC=0`: `zMcMm=1`, cioè centro locale `z=-4 mm` nel pannello spesso 10 mm. Riempimento fino alla superficie esterna, senza strati residui di plastica sopra l'apertura.

Le scanalature convergenti si fondono in **un unico volume del materiale scelto**, senza volumi fisici sovrapposti né interfacce artificiali fra riempimenti identici. Le fibre rimangono volumi distinti con identificativi 0..N−1. Il contorno di riempimento è ritagliato al pannello; raccordi cilindro/toro della fibra rimangono invariati. Le giunzioni numeriche dei tratti del groove usano una sovrapposizione interna di 1e−4 mm al solo solido unito, con deviazione dell'inviluppo inferiore alla tolleranza Geant4 ai raggi consentiti.

Gerarchia: pannello plastico → rete dei groove/riempimento → cladding → core. Le interfacce interne sono dielettriche lisce: Fresnel, rifrazione e riflessione totale dipendono dagli indici dei mezzi effettivi. Il rivestimento esterno configurato copre anche le aperture riempite a filo; **non** riveste le pareti interne della scanalatura. È un'ipotesi meccanica esplicita del modello. Il riempimento non genera luce e non ha assorbimento, scattering o WLS, secondo la semplificazione richiesta; la WLS nel core resta attiva.

I preset correnti `geant4_optics.conf`, `_diffuse.conf`, `_tyvek.conf`, `_fnal_extruded.conf`, `_ej200.conf` usano groove con gel come riferimento. Per aria e colla ci sono `geant4_optics_groove_air.conf` e `geant4_optics_groove_glue.conf`; qualunque preset corrente permette `grooveMaterial=air|gel|glue`. `grooveModel=none` conserva il modello senza groove per confronto, con posizione Z esplicita del config; i preset storici già esistenti restano senza groove. Tabulated non viene aggiornato automaticamente: le sue mappe restano quelle prodotte dal MC esterno.

## Indici documentati e ipotesi

| Chiave | Riferimento | Indice usato |
|---|---|---|
|`gel`|S24, Thorlabs G608N3, [specifiche del produttore](https://www.thorlabs.de/newgrouppage9.cfm?objectgroup_id=314&pn=ADAFCSC1)|`n=1.4338+10520/lambda_nm²`; circa 1.49204 a 425 nm, 1.48023 a 476 nm.|
|`glue`|S25, Eljen EJ-500, [datasheet luglio 2021](https://eljentechnology.com/images/products/data_sheets/EJ-500.pdf)|1.57 nominale; dispersione non fornita, trattato costante.|
|`air`|S26, [NIST, indice tipico dell'aria](https://www.nist.gov/news-events/news/2016/02/better-accuracy-out-thin-or-thick-air)|1.000277; approssimazione costante di aria ambiente, non valutazione completa di Ciddor.|

Per G608N3 i punti del produttore vanno da 402 a 1550 nm. Il fit viene usato anche per i fotoni UV, ma sotto 402 nm è **estrapolazione non validata**, visibile nei PNG e nel log. Le densità di riferimento sono 1.06 g/cm³ per G608N3 e 1.17 g/cm³ per EJ-500. Le composizioni commerciali non sono determinate dalle fonti ottiche: per la perdita di energia delle particelle si usano proxy dichiarati PDMS (`C2H6OSi`) e resina epossidica DGEBA (`C21H24O4`); nessuna scintillazione in tali volumi. L'indice del gel è campionato fra 1 e 6 eV; Geant4 ricava la velocità di gruppo dalla dispersione. Le curve di n non sono lunghezze di attenuazione.

Dati e [provenienza locale](../experiments/mine/data/optics/grooves/provenance.json); [grafici](../experiments/mine/data/optics/grooves/README.md). Implementazione: `mine_groove_materials.h`, `BuildOpticalGrooveSolid`/`BuildGeant4Optics` in `mine_geant4_optics.cc`, selezione e controlli in `mine_optical_geometry_model.*`. Materiale, versione del modello e parametri entrano nell'`opticalHash`.

## Output MC della fibra assorbente

Nuova banca **`MC::WLS` (40/6)**, una riga per ogni vero assorbimento WLS, anche se i discendenti non arrivano a un sensore. Campi: `panel`, **`fiber`**, `track`, `parent`, `time`[ns], `wavelength` assorbita[nm], `x,y,z` globali[mm], `secondaryCount`, `hitn`. La fibra è l'indice 0..N−1 del core in cui avviene l'interazione. Traccia assorbita identificata in `track`; il suo fotone riemesso ha `parent` uguale a questo ID. Ripetuti assorbimenti generano righe distinte.

In **`MINE::optical`** si aggiungono `fiber` (ultima fibra che ha assorbito via WLS), `firstWlsFiber` (prima fibra), `nWls` (numero di interazioni WLS lungo l'intera discendenza) e `entryFiber` (fibra fisica da cui arriva al sensore). Le prime due valgono −1 se non c'è stata WLS; `entryFiber=-1` per un ingresso diretto dal plastico o dal riempimento. La fibra di ingresso può differire da quella del primo/ultimo assorbimento. `entryRole=4` indica ingresso dal riempimento; i vecchi ruoli 0..3 sono conservati. L'associazione all'hit è evento+`panel`/`hitn`; per fotoni ottici senza deposito associato `hitn=-1`.

Le banche non sono riempite artificialmente nel ramo tabulato, che non possiede la storia individuale degli assorbimenti. `MINE::dgt` resta a 58 campi. Lo schema HIPO è incorporato nel file; i lettori con aspettativa rigida di 11 campi per `MINE::optical` vanno aggiornati a 15. Il controllo HIPO del progetto è stato aggiornato e ricostruisce la catena dei parent per verificare prima/ultima fibra e numero di WLS.

## Verifiche

Il volume unito usa un albero bilanciato di `G4UnionSolid`. La prima implementazione con `G4MultiUnion` intersecato al pannello è stata scartata: nel toolkit 10.7.4 dava normali di uscita nulle durante il trasporto, nonostante i controlli statici superati. Non sono stati soppressi gli avvisi.

Il test nativo `geant4_groove_geometry_test.cc` controlla sovrapposizioni, 90 punti di linea centrale, angoli del groove quadrato, apertura a filo, contenimento delle fibre, valori di n e velocità di gruppo per ogni riempimento. I test portabili verificano anche selezioni/geometrie invalide. L'integrazione GEMC/HIPO verifica la banca MC e la coerenza delle discendenze: esito e percorsi sono registrati in `reference/validation.md` al completamento.
