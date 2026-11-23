# Seed e run parallele

Input utente del 18 novembre 2026: N simulazioni GEMC da M eventi, in processi
separati in background, ciascuna con seed diverso e possibilita di scelta casuale.
Questa modifica riguarda avvio e riproducibilita, non i processi ottici.

La fonte implementativa e il codice locale `source/gemc.cc`: costruisce un
`CLHEP::MixMaxRng`, legge il seed come `G4int` e chiama
`CLHEP::HepRandom::setTheSeed`. Usa `G4RunManager` sequenziale. Non usa TRandom3.
La modalita nativa `RANDOM=TIME` deriva il seed da tempo, CPU clock e PID;
`RANDOM=0` nativo e passato a CLHEP, senza la convenzione automatica di ROOT.

Il launcher `scripts/run_mine.sh` ora sceglie un intero positivo da entropia
del sistema operativo tramite Python `secrets`, senza aggiungere ROOT fra le
dipendenze. Nel launcher, assenza di `-RANDOM`, `-RANDOM=0`, `TIME` e `auto`
significano scelta automatica. Un seed positivo esplicito resta riproducibile.
L'intervallo adottato e 1..2147483646, compatibile con G4int signed 32 bit;
il launcher passa sempre il valore numerico effettivo a GEMC, cosi e registrato
nelle condizioni di simulazione dell'HIPO e nel log. La gcard non prevale sul
seed scelto dal launcher. Il lancio diretto del binario GEMC resta invariato.

`scripts/run_mine_parallel.sh` sceglie tutti i seed prima dell'avvio e rimuove
le collisioni all'interno del batch. `--seed-base B` produce B,B+1,..., se si
vuole riprodurre la selezione dei seed; default/0 usa nuovi seed casuali.
Seed distinti non sono una modifica o una dimostrazione delle proprieta
statistiche del motore. La riproducibilita richiede anche stessa geometria,
configurazione, software e input: il batch copia gcard e tabella/config ottico,
registra hash, comandi, seed, input e stati nel manifest. TEXT, sorgenti e
file LUND non sono congelati; i LUND condivisi vengono riletti dall'inizio
in ciascuna run, non suddivisi. Non cambiare questi input durante il batch.

File HIPO separati vengono finalizzati prima di entrare in `inputs.list`.
Il supervisore non unisce configurazioni diverse; l'analisi puo usare
`--list inputs.list --merge` per i campioni compatibili. Un batch fallito non
produce `inputs.list`; `completed.list` contiene soltanto le run riuscite.
Lo stop e selettivo per ID di run: non interrompe simulazioni estranee.
