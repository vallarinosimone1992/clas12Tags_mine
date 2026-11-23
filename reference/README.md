# Registro delle fonti e delle decisioni MINE

Ricostruito il **12 novembre 2026** dai sorgenti, dalle configurazioni, dai file di provenienza e dalla conversazione con Simone. Le date dei singoli messaggi non sono ricostruibili: non vengono inventate. Il registro riguarda la simulazione GEMC in questo repository; il MC ottico e l'analisi esterni sono indicati come dipendenze, senza certificarne ogni parametro interno.

- [Fonti](sources.md): bibliografia, grafico/pagina utilizzati, impiego e limiti.
- [Decisioni e parametri](decisions.md): valori, posizione nel codice, motivazione e modalità in cui sono attivi.
- [Input dell'utente](user_inputs.md): richieste esplicite distinte dalle ipotesi introdotte nel modello.
- [Ricerca sul Rayleigh](rayleigh.md): lunghezze assolute nel PS, dipendenza da λ e limiti di trasferimento; ancora non implementato.
- [Groove e fibre assorbenti](grooves.md): geometria, indici del riempimento e campi MC/ottici.
- [Analisi HIPO compilata con clas12root](analysis.md): progetto parallelo, definizioni delle grandezze e verifiche.
- [Verifiche](validation.md): evidenza della compilazione e dei controlli effettuati.
- [Inventario della versione](snapshot.json): valori dei config, parametri TEXT, percorsi e SHA256 degli input locali alla data del registro. È una fotografia, non sostituisce il config del singolo run.

- [Diagnosi del run 0007](run_0007_20261120.md): fotone WLS quasi trasversale e perdite di fibra disabilitate; stato salvato e analisi dei run completi.

- [Dati del meeting Kuraray del 13 novembre 2026](manufacturers/kuraray_20261113/assessment.md): copie originali, proprieta estraibili e limiti per futuri miglioramenti; non applicati alla baseline.

## Come leggere la provenienza

| Etichetta | Significato |
|---|---|
| Documentato | Valore/curva presenti nella fonte citata; applicabilità al componente MINE da verificare. |
| Derivato | Calcolo o digitalizzazione riproducibile da una fonte; le assunzioni sono dichiarate. |
| Input utente | Scelta richiesta da Simone; non implica una misura di laboratorio. |
| Ipotesi | Valore o legge non calibrati, adottati per far funzionare/studiare il modello. |
| Ereditato/accettato | Presente nel codice precedente e mantenuto, eventualmente con assenso dell'utente. |
| Storico | Ancora disponibile per confronti; non è la scelta corrente del trasporto esplicito. |
| Contesto/non importato | Consultato per interpretare o scartare una scelta; non è un input numerico attivo. |

Le categorie possono coesistere: una curva di datasheet è documentata, la sua digitalizzazione è derivata, il trasferimento a un diverso contatto ottico è un'ipotesi.

## Distinguere le modalità

**`tabulated` resta il default del launcher**: geometria, raccolta e tempi ottici vengono dalla tabella selezionata in `experiments/mine/responses/optical_response.dat`. Cambiare un rivestimento nel config Geant4 non modifica quella tabella. La provenienza effettiva della risposta va letta nei metadati del file e del run.

**`geant4`** usa i config ottici, i volumi core/cladding e le curve descritte qui. Supporta corner fan e `mini_panel_spiral`, profili Y-11/YS-6/YS-4 single cladding e due readout. `sipmModel=ideal` è il default esplicito; `sipmModel=hamamatsu` applica PDE spettrale e modello semplificato di celle/rumore. La scelta nel config ottico prevale su `ideal_SiPM` nel TEXT. `mini_panel_spiral` è disponibile solo in questa modalità: il launcher rifiuta il trasporto tabulato per questa geometria.

**Legacy efficace**, con tabella disattivata e senza trasporto esplicito: conserva fattori scalari, cammini geometrici approssimati e profili storici. I suoi parametri non vanno applicati di nuovo ai fotoni tracciati o a una tabella che li include.

## Dati e riproducibilità

I CSV e le curve compilate restano accanto alla simulazione in [data/optics](../experiments/mine/data/optics). I relativi `provenance.json` contengono hash dei documenti, calibrazione degli assi, convenzioni e limitazioni. Gli script `extract_mine_*_spectra.py` in [scripts](../scripts) permettono di rigenerarli da copie locali delle fonti. Il registro centrale li collega, senza duplicare indiscriminatamente i PDF.

Per ogni futura modifica fisica aggiornare fonte/input, formula, unità, file/chiave, modalità attiva e limiti; conservare le scelte superate come storico. Non trasformare una richiesta dell'utente o un test numerico in una validazione sperimentale.

## Grafici delle reference

La [galleria PNG](plots/README.md) raccoglie tutti i CSV spettrali: emissione/trasmittanza degli scintillatori, assorbimento/emissione Y-11, riflettanza dei rivestimenti e PDE Hamamatsu. Dimensioni 1800×1200 px, asse λ 250–900 nm; ordinate relative/probabilità 0–1. La lunghezza WLS derivata ha asse verticale logaritmico in mm. Ogni PNG ha titolo/materiale, unità, fonte e limiti; copie identiche sono accanto ai CSV.

Rigenerazione: `python3 scripts/plot_mine_references.py`. [Manifest dei grafici](plots/manifest.json): hash degli input, scale e copie. La generazione dei plot non modifica dati o parametri della simulazione.

La gestione dei seed e dei processi paralleli e documentata in [random_seeds.md](random_seeds.md): input utente, sorgente locale del generatore, intervallo e tracciabilita delle run.

- [Profili WLS e perdite comuni, 20 novembre 2026](wls_fibers.md): Y-11, YS-6, YS-4, spettro/decay distinti, ipotesi comuni, benchmark Geant4 e HIPO.

- **Corrente, 22 novembre 2026:** [perdite spettrali Kuraray e budget WLS](wls_spectral_loss.md), proxy Y-11, verifica monocromatica e discrepanza della diagnostica integrale. Sostituisce i 4 m costanti del 20 novembre.

- **Mini-panel, 23 novembre 2026:** [geometria a spirale e configurazione](mini_panel_spiral.md), pannello 300×500×8 mm con due fibre Y-11 Non-S, EJ-200, Mylar e gel; [proprietà dei materiali e limiti](mini_panel_materials.md), valori nominali EJ-200, rise time nativa e assorbimento bulk ancora provvisorio.

- [Config predefinito incorporato negli HIPO](hipo_config.md): percorso originale, copia del contenuto, priorità nell’analisi e compatibilità con i file precedenti.
