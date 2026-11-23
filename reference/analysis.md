# Analisi HIPO MINE con clas12root

Stato: 17 novembre 2026. Progetto parallelo [simulation_time_resolution](../../simulation_time_resolution/README.md), con eseguibile C++ compilato e collegato a ROOT/clas12root. La simulazione e i suoi input non sono modificati dall'analisi.

Il symlink `simulation_time_resolution/data` punta a `../clas12Tags_mine/experiments/mine/output`. Selezione esplicita per file/lista, risultati in `simulation_time_resolution/results`, build in `build/native`. Il prefisso Docker `/work/clas12Tags_mine/` viene tradotto nel percorso host del progetto GEMC.

L'utente ha confermato tempi e diagnostica ottica: TDC/cariche dei sensori, mappe XY, fotoni generati/arrivati e fibra WLS. Il riferimento dei tempi resta `MINE::dgt.t0`; non si sottrae nuovamente il volo diretto. Si distinguono assorbimenti ripetuti da prime catture e risoluzione del timestamp dell'evento da dispersione dei tempi dei singoli fotoni. La raccolta verso il sensore include il trasporto ottico: non equivale al trapping condizionato nel core.

Metodi, fonti tecniche locali e input dell'utente sono nel [registro dell'analisi](../../simulation_time_resolution/reference/README.md), con [verifiche](../../simulation_time_resolution/reference/validation.md). Ogni risultato compilato conserva config, input e hash del binario/sorgenti/libreria in `analysis_build.json`.

Il primo campione `mine_20261112_172403_54158.hipo` contiene 20 eventi e serve come verifica funzionale. I fit e la calibrazione non vengono forzati riducendo le soglie statistiche. I risultati sono nella [cartella dedicata](../../simulation_time_resolution/results/mine_20261112_172403_54158).


## 23 novembre 2026: geometria automatica

L'analisi riconosce layout HIPO 3 e 7 e recupera il config ottico con hash
esatto dai log/snapshot/preset. Mini spiral e gap gel sono ora supportati;
vecchi HIPO utilizzabili senza rigenerazione. Quote nominali 1000×1000×10 e
300×500×8 mm; Box/centri custom richiedono modo manuale, poiché non sono
registrati integralmente nelle banche esistenti. Nessun aggregato di hash o
layout diversi viene accettato. Verifiche numeriche e dettagli nel
[README analisi](../../simulation_time_resolution/README.md).
