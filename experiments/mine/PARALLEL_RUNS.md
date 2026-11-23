# Simulazioni parallele e analisi di piu file

Dalla radice di `clas12Tags_mine`:

```sh
# N=4 processi simultanei; M=1000 eventi in ciascuna run (4000 in tutto).
./scripts/run_mine_parallel.sh --runs 4 --events 1000 -- --transport geant4

# N=8 run, ma al massimo 4 simultanee; le altre attendono in coda.
./scripts/run_mine_parallel.sh --runs 8 --events 1000 --jobs 4 -- --transport geant4

# Tutti i consueti parametri del launcher/generatore vanno dopo --.
./scripts/run_mine_parallel.sh --runs 4 --events 1000 -- --transport geant4 \
  --geometry corner_fan --optical-config experiments/mine/geant4_optics.conf \
  '-BEAM_P=mu-, 10*GeV, 0*deg, 0*deg' \
  '-BEAM_V=(0, 0, -100)mm' '-SPREAD_V=(500, 500, 0)mm'
```

Il trasporto Geant4 di questa versione e sequenziale. I processi separati
possono usare piu core della VM Docker: oltre ai core assegnati a Docker
non ci si aspetta ulteriore accelerazione, e ogni processo richiede memoria.
`--jobs` e il limite dei processi di questo batch, senza contare run esterne.
Il trasporto predefinito resta `tabulated`, come in `run_mine.sh`: specificare
`-- --transport geant4` per il trasporto esplicito dei fotoni.

Il comando ritorna dopo aver avviato il supervisore. Le simulazioni continuano
anche chiudendo il terminale: non aggiungere `&` o `nohup`. `--wait` mantiene
il comando in attesa della conclusione e restituisce errore se una run fallisce;
in questa modalita Ctrl+C richiede lo stop di questo batch. Con l'avvio
distaccato usare `--stop` per interromperlo.

## Opzioni

| Opzione | Significato |
|---|---|
| `--runs N`, `-n N` | Numero di simulazioni. |
| `--events M`, `-m M` | Eventi per simulazione, totale N×M prima di eventuali filtri. |
| `--jobs J`, `-j J` | Massimo di processi simultanei; default N. |
| `--seed-base B` | Seed B,B+1,... per riprodurre il gruppo; default/0 sceglie casualmente. |
| `--output-dir DIR` | Nuova cartella del batch, dentro il progetto montato in Docker. Se esiste viene rifiutata. |
| `--dry-run` | Valida impostazioni e salva piano/seed/comandi, senza avviare processi o richiedere Docker. Per eseguire un piano usare gli stessi seed in una nuova cartella; non esiste un resume automatico. |
| `--wait` | Attende e restituisce l'esito del batch. |
| `--status DIR` | Mostra stato, seed, eventi ed ETA disponibili per ogni run. |
| `--stop DIR` | Richiede l'arresto delle sole run di questo batch, comprese quelle in coda. |
| `--help` | Help completo. |

Dopo `--` sono accettati gli argomenti di `run_mine.sh`: card, geometria,
trasporto, config ottico, tabella, generatore e progresso. GUI, TXT, `-N`,
`-RANDOM`, `-OUTPUT` e `-EXEC_MACRO` sono gestiti/rifiutati dal supervisore per
preservare M eventi, seed distinto e HIPO separato per run. I nomi con spazi e
le opzioni contenenti parentesi vanno quotati; non viene usato `eval`.
Un `--lund FILE` condiviso viene riletto dall'inizio in ciascuna run: non e una
suddivisione automatica del file fra processi.

## Seed, input e risultati

I seed automatici vengono scelti prima dell'avvio da entropia del sistema
operativo e sono distinti all'interno del batch. Non dipendono dalla risoluzione
dell'orologio e non richiedono ROOT. In ogni caso sono passati a GEMC come
interi positivi e memorizzati nelle condizioni HIPO, nei log, in `seeds.tsv`
e nel `manifest.json`. L'intervallo e 1..2147483646. Un seed uguale riproduce
la sequenza solo a parita di software, input e configurazione.

La cartella stampata dal comando contiene:

- `run_0001.hipo`, ... e `run_0001.log`, ...: output e log di ogni processo.
- `batch.log`: avvio/fine di ogni run ed eventuali errori del supervisore.
- `manifest.json`: seed, comandi, ID/PID, tempi UTC, stato e ultimo progresso.
- `seeds.tsv`, `commands.sh`: elenco dei seed e comandi per riprodurre le run.
- `inputs/`: copie della gcard e del config/tabella ottica, con hash nel manifest.
- `inputs.list`: lista dei file finalizzati, presente solo se tutte le run riescono.
- `completed.list`: a fine batch, soltanto i file delle run riuscite, anche se altre falliscono.

Anche i log GEMC standard restano in `experiments/mine/output/mine_ID.log`
(indicati nel manifest). I processi hanno output e ID distinti: non modificano
il symlink ottico selezionato e non sovrascrivono cartelle di batch preesistenti.
I file TEXT, il binario, le curve ottiche compilate e gli eventuali LUND non
vengono congelati: non modificarli mentre un batch e in esecuzione.

Per controllare o fermare un gruppo, usare la cartella stampata all'avvio:

```sh
./scripts/run_mine_parallel.sh --status experiments/mine/output/batch_DATA_ID
./scripts/run_mine_parallel.sh --stop experiments/mine/output/batch_DATA_ID
```

Lo stop non interrompe altre simulazioni e non spegne Docker/VNC. Gli HIPO
interrotti possono essere incompleti e non entrano nelle liste di successo.
`--status` mostra l'ultimo progresso disponibile: durante un singolo evento
lungo il contatore/ETA restano quelli dell'evento precedente.

## Analisi con clas12root

Il software di analisi accetta gia piu input o una lista. Dalla cartella
`simulation_time_resolution`, parallela al progetto GEMC:

```sh
# Un unico campione statistico, per run con la stessa configurazione.
./scripts/run_analysis.sh \
  --list ../clas12Tags_mine/experiments/mine/output/batch_DATA_ID/inputs.list \
  --merge --output corner_fan_4x1000

# Oppure indicare i file singolarmente.
./scripts/run_analysis.sh --input data/run1.hipo data/run2.hipo \
  --merge --output corner_fan_unito
```

L'analisi usa ora un dataset globale per default, con tempi, fit e diagnostica
ottica dell'intero campione. `--merge` (alias `--chain`) lo rende esplicito;
`--separate` richiede campioni indipendenti per file. Restano i controlli di
compatibilita delle configurazioni. Cambiare solo il seed e compatibile. Analizzare soltanto HIPO chiusi. Il nome del
risultato riprende quello della lista/input se `--output` e assente; poiche
la lista del supervisore si chiama sempre `inputs.list`, specificare un nome
`--output` per distinguere piu batch. I risultati esistenti non vengono sovrascritti.

## Verifica

`python3 scripts/test_mine_parallel.py` controlla seed, concorrenza, coda,
output/lista, errore parziale, distacco e stop selettivo con processi fittizi,
senza eseguire GEMC. `python3 scripts/test_mine_launch.py` verifica anche
la risoluzione dei seed e la validazione senza Docker.
