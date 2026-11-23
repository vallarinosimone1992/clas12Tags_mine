# Input dell'utente e decisioni accettate

Fonte U01: messaggi di Simone in questa conversazione, ricostruiti il 12 novembre 2026. Parafrasi fedeli; date dei messaggi non disponibili. Un valore richiesto è una scelta di progetto, non viene qualificato come misurato.

| Richiesta / decisione | Stato e distinzione dalla letteratura |
|---|---|
| Scintillatore estruso: 8000 fotoni/MeV, decadimento 2.5 ns, attenuazione bulk 100 cm, velocità efficace bulk 17 cm/ns | Input espliciti. Gli ultimi due sono valori efficaci: nel trasporto esplicito la velocità deriva dall'indice, mentre 1000 mm viene mantenuto come ipotesi di assorbimento uniforme lungo il raggio. |
| «Birks va bene» | Accettazione del precedente 0.126 mm/MeV; coefficiente ereditato da GEMC, non misura fornita dall'utente. |
| Y-11 single cladding; futura opzione YS-6 | Tipologia richiesta, con chiarimento YS-6 invece di “Y6”. Concentrazione Y-11 200 ppm e diametro nominale 1 mm restano riferimenti di modello da confermare sul componente. |
| Circa 6% dalle specifiche, 3% per verso | Interpretato correttamente come trapping condizionato all'emissione isotropa nel core; usato 0.031 per estremità da catalogo, non 6% come efficienza globale di intercettazione dal pannello. |
| Coupling efficiency 1; gain 1 | Input espliciti: accoppiamento ideale e unità di carica normalizzata. Non equivalgono a una misura di coupling o al gain fisico Hamamatsu. |
| `ideal_SiPM` falso di default, alternativa ideale selezionabile | Rispettato nei rami tabulato/legacy. Il ramo esplicito usa ora `sipmModel=ideal|hamamatsu`: il default ideale è stato richiesto successivamente per il mini-panel, con alternativa Hamamatsu. La precedente imposizione ideale della prima fase resta storica. |
| Digitalizzazione mantenuta ai valori esistenti | Accettazione provvisoria, non parametri di elettronica misurati. |
| Jitter intrinseco SiPM ed elettronica trascurabile per ora | Zero aggiuntivo; l'analisi può riportare risoluzioni con contributi aggiunti alla varianza. Non confondere questo con un modello di SPTR per singolo fotone. |
| Readout comune rettangolare circa 20×1.5 mm², uguale per tutte le configurazioni | Proxy geometrico richiesto. Auto-fit esplicito può aumentarne la larghezza per le impronte oblique (15 fibre: 20.5012 mm); non corrisponde all'area attiva 3×3 mm² del SiPM di riferimento. |
| Numero delle fibre flessibile, incluse 15; in caso di incroci preferire sfalsamento verticale a curve strette | Requisito geometrico. Il corner fan esplicito corrente ha 15 fibre e nessuno sfalsamento verticale automatico; offset Z per fibra configurabili con controlli geometrici. |
| Clear mantenuta come opzione storica, senza concentrare il lavoro sul migliorarla | Ramo MC/legacy storico. Il trasporto esplicito attuale supporta Y-11/YS-6/YS-4, senza nuova implementazione clear. |
| Ottenere tempi/raccolta completi, evitare il doppio conteggio del trasporto MC e GEMC | Motivazione della separazione tabulato/esplicito e del contratto dei metadati. Nessun ulteriore fattore di trapping, ritardo di propagazione o decadimento WLS quando già incluso nella risposta usata. |
| Spettri alternativi FNAL estruso ed EJ-200 | Implementati come selezione dello spettro; i valori di resa e decadimento richiesti non sono cambiati automaticamente. Il materiale generico GEMC non diventa PS scegliendo il solo spettro FNAL. |
| Diffusore TiO₂ per estrusi e Tyvek; Mylar alluminato speculare con fonti affidabili | Implementati come materiali configurabili. Il precedente 98% non è un valore sperimentale fornito dall'utente e non viene presentato come tale. Il suggerimento iniziale di uguagliare le sopravvivenze è stato superato dalla richiesta di cercare valori specifici. |
| Possibile corrispondenza fra massimo riflettività e massimo emissione FNAL | Domanda di ricerca, non autorizzazione a traslare artificialmente una curva misurata. Curve implementate alle lunghezze d'onda originali. |
| Variation di geometria per scegliere il rivestimento in seguito | Per ora selezione tramite file ottico; non introdotte nuove variation per i rivestimenti. |
| Tenere traccia di tutte le reference e delle giustificazioni, inclusi gli input utente | Questo registro e i metadati locali attuano la richiesta; le future modifiche fisiche devono aggiornarli. |
| Per ora riassorbimento e riemissione radiativa nel plastico trascurabili | Input esplicito del 12 novembre 2026: mantiene assente questa coppia di processi. Non implica disattivare la WLS nella fibra né azzerare automaticamente la perdita bulk non radiativa di 1000 mm. |

| Groove quadrati 2×2 mm con fibra centrata e riempimento gel/aria/colla fino alla superficie | Input del 12 novembre 2026, confermato con groove aperti sulla faccia originaria delle fibre: centro a 1 mm, profondità 2 mm. Assorbimento nel riempimento trascurabile. Gel G608N3 scelto come riferimento implementativo, non come componente acquistato dall’utente. |
| Salvare quale fibra ha assorbito il fotone, preferibilmente nella banca MC | `MC::WLS.fiber` per ogni assorbimento, con parentela; prima/ultima fibra e conteggio WLS anche negli arrivi `MINE::optical`. |

## 20 novembre 2026: fibre WLS

Simone autorizza proprieta comuni dove non documentate come diverse, perdita
efficace comune a core/cladding, Y-11 e YS-6 separate e YS-4 suggerita da Kuraray.
Richiede implementazione GEMC, benchmark su fibra diritta e documentazione.
Il valore comune 4000 mm e la scala YS trasferita da Y-11 sono ipotesi di
implementazione, non misure fornite dall’utente. Vedi [WLS](wls_fibers.md).

## 22 novembre 2026: curve Kuraray in lambda

Simone richiede di usare le curve, comuni a core/cladding, senza doppio WLS,
con verifica della fibra diritta e documentazione della sostitutiva Y11.
La media dei coefficienti YS4/YS6 per Y11 e la separazione a 520 nm con raccordo
a 480 nm sono decisioni del modello, non nuovi dati forniti dall’utente.

## 23 novembre 2026: mini-panel a spirale

L'utente richiede l'implementazione di `MC_MINE_LightTrapping/config/mini_panel_spiral.conf`
con tutte le proprietà ottiche avanzate già disponibili. La geometria sorgente
fornisce il pannello 300×500×8 mm, due fibre sovrapposte da 1 mm, groove 1×2 mm,
cladding radiale 0.03 mm e due readout 3×3 mm: non sono dimensioni ricavate dai
datasheet ottici.

| Scelta esplicita | Applicazione e distinzione dalle ipotesi |
|---|---|
| Plastica colata tipo EJ-200 | Spettro EJ-200; resa 10000 fotoni/MeV, rise 0.9 ns, decay 2.1 ns e densità 1.023 g/cm³ dai valori nominali Eljen. Bulk microscopico 1000 mm resta una scelta provvisoria, non il dato tecnico di attenuazione di 380 cm. |
| Fibra Y-11 Non-S | Profilo spettrale Y-11 attuale, single cladding, senza modificare il budget di assorbimento già documentato. Raggio interno 30 mm dalla geometria sorgente: tenuta meccanica/perdite da stress non validate per Non-S. |
| Mylar alluminato e gel ottico del riferimento attuale | Curva Mylar già disponibile; G608N3 nel groove e nel gap SiPM. Nessuna nuova misura di riflettività assoluta o assorbimento del gel. |
| S esterna fra pannello e riflettente, aria nel rialzo, rampe a 30° | Ricostruzione geometrica del rivestimento che si solleva localmente e ritorna sul pannello; dettagli numerici e ipotesi in [mini_panel_spiral.md](mini_panel_spiral.md). |
| Gel di accoppiamento spesso 1 mm; SiPM possono vedere anche il plastico | Nessuna maschera che accetti solo fotoni provenienti dalle fibre. Gli arrivi diretti restano distinguibili dalla verità ottica. |
| SiPM ideale corrente, con opzione Hamamatsu e modello semplificato | `sipmModel=ideal` nel preset mini; preset separato `sipmModel=hamamatsu`. Jitter aggiuntivo ancora zero. |

La modalità mini è implementata nel trasporto esplicito Geant4; la selezione
`mini_panel_spiral` con `tabulated` viene rifiutata. Il generatore del suo gcard
predefinito distribuisce muoni uniformemente su 300×500 mm e li dirige lungo +Z.
Le ipotesi sui materiali sono raccolte in [mini_panel_materials.md](mini_panel_materials.md).
