> **Registro corrente delle fonti e delle decisioni:** [reference/README.md](../../reference/README.md). Questo documento conserva il modello efficace storico del 15 ottobre 2026; per il trasporto esplicito valgono i config e il registro corrente, inclusa l'eccezione del SiPM forzato ideale.

> Explicit Geant4 transport is available via `--transport geant4`; its separate
> material conventions, required assumptions and validation status are described
> in [GEANT4_OPTICS.md](GEANT4_OPTICS.md). The legacy factors below are not used
> for traced photons.
>
> Current default: the HitProcess uses the self-contained optical MC table selected
> by `responses/optical_response.dat`. The scalar collection/trapping, nearest-fiber
> routing, bulk/fiber delays and attenuation described below apply only to the
> legacy branch (`mine_use_optical_table=0`). In the default branch those effects
> come from the table; only a declared missing WLS decay is added. Birks,
> scintillation, coupling, SiPM and electronics remain active. See README.md for
> the dual-sensor bank, metadata contract and host launcher.

# Modello fisico efficace MINE: aggiornamento 15 ottobre 2026

## Scintillatore estruso

Sono adottati i valori richiesti: Birks 0.126 mm/MeV, resa 8000 fotoni/MeV di
energia visibile, decadimento 2.5 ns, attenuazione bulk 1000 mm e velocità bulk
170 mm/ns. Sono ipotesi di lavoro MINE, non misure certificate del materiale.
La geometria Geant4 e il materiale generico restano quelli già presenti.

## Cattura, riemissione e trapping sono quantità distinte

Per una lettura WLS a una estremità il modello usa, per ciascuno step:

```
N_gamma = Evis * light_yield
N_capture = N_gamma * collection_efficiency * exp(-d_bulk / lambda_bulk)
N_sensor = N_capture * trapping_per_end * coupling * exp(-s_end / lambda_fiber)
mean_Npe = N_sensor * PDE(lambda_emission)
```

`collection_efficiency` include l'intercettazione/cattura efficace e la resa di
assorbimento/riemissione PRIMA del trapping. Il catalogo non determina questa
probabilità per un pannello con un dato rivestimento e un dato percorso di fibre.
Il precedente 1% resta provvisorio; non è stato sostituito dal 6%.

Per fibra tonda single cladding, Kuraray riporta 3.1% di trapping. È la frazione
per verso della luce emessa isotropicamente nel core: circa 6.2% sommando i due
versi prima di attenuazione e accoppiamento. Si applica 0.031 alla singola
estremità selezionata. La rifrazione e il trapping di una fibra clear sono
invece dipendenti dalle direzioni incidenti: non si usa lo stesso fattore WLS.

Un ulteriore `wls_efficiency` privo di definizione precisa renderebbe ambigua la
normalizzazione. È stato eliminato. L'assorbimento e la resa quantica WLS sono
ancora fisicamente necessari, ma vengono assorbiti nell'efficienza di cattura.
Quando si importerà una mappa del MC occorrerà dichiarare se include già il
trapping: in tal caso NON si deve moltiplicare nuovamente per 0.031.

## Perché una geometria serve anche senza ottica Geant4

Non serve costruire volumi ottici e tracciare ogni riflessione in GEMC per usare
una risposta efficace. Serve però conoscere la dipendenza della risposta dalla
posizione del deposito e dall'estremità letta.

La risposta può essere una funzione/tabella K(x,y,z,sensore,tempo). Questa
funzione incorpora la geometria, anche se il programma che la usa non contiene
alcun volume di fibra. In alternativa si può stimare temporaneamente la risposta
con distanze geometriche. Le due cose non sono equivalenti in accuratezza.

Esempio: un deposito vicino al bordo superiore può essere vicino alla fibra,
ma molto lontano dall'estremità inferiore seguendo una serpentina. Una sola
distanza rettilinea deposito-SiPM confonderebbe il tragitto nella plastica con
quello nella fibra, e applicherebbe attenuazioni e ritardi sbagliati. Il modello
separa dunque d_bulk e s_end, lunghezza lungo il percorso della fibra.

Sono state riprese le formule dei percorsi `parallel_fan_in` e
`double_serpentine` da MC_MINE_LightTrapping, senza modificarlo. Il punto di
raccolta provvisorio è il punto più vicino sulla linea centrale della fibra;
la posizione del sensore è l'estremo del percorso, non il precedente bordo +X.
Il modello attuale sceglie una sola estremità per esecuzione.

Nel fan-in corrente, cinque estremi separati di 1.2 mm occupano 4.8 mm fra i
centri estremi, più il diametro della fibra: non entrano su un SiPM 3 x 3 mm
se disposti su una sola fila. Serve verificare il fascio/impacchettamento 2D o
un accoppiatore. Il coupling unitario richiesto resta una condizione ideale;
non certifica che l'intero fascio sia accoppiato all'area attiva.

Questa approssimazione NON calcola l'efficienza geometrica: un fotone reale può
riflettersi molte volte e raggiungere una fibra diversa o un altro punto della
stessa fibra. La distanza bulk minima non è il cammino ottico medio. Pertanto
l'attenuazione bulk attuale è una stima basata sul cammino minimo e il timing non
include la dispersione dei cammini. L'esportazione delle mappe/distribuzioni del
vostro MC resta il passaggio necessario per sostituire questa approssimazione.
Le mappe finali dovrebbero conservare almeno posizione, fibra/punto di cattura,
estremità, cammino bulk, cammino in fibra e tempo; una sola efficienza media
non basta per ricostruire attenuazione e distribuzione dei tempi.

Se si importerà direttamente la distribuzione dei tempi di arrivo del MC,
non si dovranno aggiungere di nuovo propagazione e decadimento WLS già inclusi.

## Profili fibra e provenienza

Profilo attivo: Y-11(200) single cladding. Si assume concentrazione 200 ppm e
sezione tonda come riferimento; diametro e tipo S/Non-S vanno confermati sul
componente acquistato. Il routing usa il limite di curvatura Non-S per 1 mm.

- Emissione rappresentata dal picco 476 nm.
- Attenuazione efficace iniziale 3500 mm: scelta conservativa dalla specifica
  `>3.5 m`, non una misura esatta. Il singolo esponenziale è una semplificazione,
  soprattutto alle brevi distanze. Il tipo S può richiedere valori differenti.
- Decadimento Y-11 6.9 ns, valore di lavoro dalla presentazione tecnica Kuraray.
- Accoppiamento 1 come richiesto.
- Indici core/cladding 1.59/1.49. Da questi si ricava una stima della velocità
  efficace, non un valore misurato o garantito dal produttore.

Per emissione isotropa selezionata nel cono guidato meridionale, cos(theta) è
uniforme fra r=n_clad/n_core e 1. La media dei tempi per unità di lunghezza è:

```
<1/v_axial> = (n_core/c) * log(1/r)/(1-r)
v_eff = (c/n_core) * (1-r)/log(1/r) ~= 182.56 mm/ns
```

Si usa questa velocità lungo la linea centrale e l'attenuazione catalogata lungo
la fibra, senza un ulteriore fattore di cammino obliquo: quest'ultimo conterebbe
nuovamente un effetto già presente nella misura di attenuazione. Dispersione,
indice di gruppo e perdite specifiche delle curve non sono modellati.

Il profilo futuro YS-6 si seleziona con `mine_fiber_model=1`: picco 462 nm,
attenuazione di lavoro 3000 mm dal limite catalogato `>3 m`, decadimento 1.47 ns
misurato da Kodama et al. Il decadimento NON è una specifica del produttore.
Applicare questi dati alla futura variante single clad è un'ipotesi da verificare.
Il trapping resta quello single clad; non si importa il trapping multicladding.

Fonti:

- [Catalogo Kuraray: materiali e strutture, pagine stampate 3-4; WLS, pagina 7](https://methacrylate.kuraray.com/content/dam/methacrylate/ja/documents/psf.pdf).
- [Catalogo WLS aggiornato, inclusa la serie YS](https://methacrylate.kuraray.com/ja/products/psf/wavelength-shiftingfiber/).
- [Presentazione tecnica Kuraray 2019, decadimento Y-11](https://indico.cern.ch/event/856756/attachments/1967422/3271748/Kuraray_PSF_new_development_20191205.pdf).
- [Kodama et al., PTEP 2024, 053H01](https://academic.oup.com/ptep/article/2024/5/053H01/7644353).

## SiPM e tabella PDE

Si assume la variante CS del S13360-3075: un canale 3 x 3 mm, passo 75 um,
1600 microcelle. Il modello usa dark count tipico 500 kHz e cross-talk 7% alle
condizioni nominali di 25 C e sovratensione 3 V. Il guadagno fisico nominale
4e6 elettroni è documentato ma il parametro di simulazione rimane 1.
Capacità nominale 320 pF, tensione di breakdown tipica 53 V e coefficiente
termico 54 mV/C sono dati di progetto, non nuovi parametri attivi del modello.

La curva PDE è stata digitalizzata dalla traccia rossa CS del grafico
KAPDB0325EB a pagina 4, pannello 75 um. Si usa interpolazione lineare; i numeri
sono approssimati, con incertezza indicativa di un punto percentuale assoluto.
L'ancora nulla a 270 nm è imposta; la traccia leggibile inizia circa a 280 nm.
La PDE del grafico comprende già il fill factor e non include cross-talk o
afterpulse. Lo spettro completo non è campionato: ogni pacchetto usa il picco
WLS, oppure il proxy scintillatore 425 nm per lettura diretta/riferimento.
Questa è un'approssimazione dichiarata, non una PDE integrata sullo spettro.

Il datasheet non quantifica probabilità e tempi degli afterpulse, né il recupero
delle microcelle. Gli afterpulse restano disattivati; la saturazione conserva
il modello esistente di occupazione senza recupero durante l'hit. Il cross-talk
resta a una generazione, non una cascata.

`ideal_SiPM=1` impone PDE unitaria e disabilita saturazione, cross-talk,
afterpulse e dark count. Mantiene la statistica poissoniana dei fotoni,
i tempi ottici, il gain configurato e l'elettronica. Il valore predefinito è 0.

Fonte: [Datasheet Hamamatsu S13360, pagine 2-4](https://hep.hamamatsu.com/content/dam/hamamatsu-photonics/sites/documents/99_SALES_LIBRARY/ssd/s13360_series_kapd1052e.pdf).

SHA-256 dei PDF consultati:

```
Hamamatsu: b657b746e41bcdb2f8b97c5ec054816130236ad5f0f6ebece5a7a7a766325098
Kuraray:   69f43b74ce1d0d0b38b9930dbc2c44452e068bd6bdb768bb1034facc9ef57610
```

## Digitalizzazione

Tutti i valori e gli algoritmi preesistenti di digitalizzazione sono conservati.
L'integrale ADC è continuo e normalizzato; la soglia opera sull'ampiezza della
forma d'onda. Il TDC è il primo attraversamento crescente interpolato.


## Aggiornamento geometrie del 19 ottobre 2026

Sono disponibili `diagonal_corner_fan` (codice 3), `triple_serpentine` (4),
la doppia serpentina adattata al pannello (5) e la fibra rettilinea (6).
Il fan-in parallelo (1) usa ora le curve compatte aggiornate del MC; il codice 2
mantiene la vecchia doppia serpentina dimensionata manualmente.

La configurazione corner-fan replica `quick_corner_fan.conf` e
`grid_corner_fan.conf`: 14 fibre, senza fibra centrale, vertici virtuali sulla
diagonale trasversa separati di 100 mm, due estremità sugli angoli opposti.
Ogni percorso è rettilineo + raccordo circolare + rettilineo. Il raggio del
raccordo è 1.01 volte Rmin, come nell'originale, per evitare falsi errori di
curvatura dovuti alla discretizzazione. Si può selezionare l'altra diagonale.

La tripla serpentina porta entrambe le estremità di ciascuna fibra sullo stesso
bordo: l'indice di estremità continua a identificare il sensore, non il segno
della coordinata Y. La doppia serpentina nuova calcola automaticamente il numero
di corsie e supporta orientamento orizzontale o verticale.

I percorsi derivano dalle effettive dimensioni del Box sensibile. Restano attivi
il modello di cattura nel punto più vicino e l'efficienza provvisoria già
illustrati sopra; aggiungere geometrie non aggiunge le efficienze ottiche del MC.
Le configurazioni dedicate mantengono scintillatore, fibra Y-11, SiPM e
l'elettronica del riferimento. I raggi di curvatura e i limiti del pannello sono
verificati prima di usare i percorsi.
