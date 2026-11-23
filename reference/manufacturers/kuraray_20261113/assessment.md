**Modello corrente (22 novembre):** [curve spettrali e separazione WLS](../../wls_spectral_loss.md).

# Dati Kuraray per la simulazione MINE

Valutazione del 20 novembre 2026. Le copie dei due documenti sono identiche agli
allegati dell'utente (hash in provenance.json). Questa nota confronta dati e
modello; **nessun nuovo valore e stato implementato**. La baseline di 46000 eventi
e congelata separatamente con simulation_conditions.txt.

**Aggiornamento successivo:** i profili e le approssimazioni ora implementati sono
descritti in [wls_fibers.md](../../wls_fibers.md). Il testo seguente conserva
la valutazione iniziale; nessun nuovo valore e stato applicato retroattivamente
alla baseline di 46000 eventi.

## Fonti e condizioni di misura

- [Kuraray, meeting del 13 novembre](kuraray_psf_meeting_20261113.pdf): pagine
  5-8 per materiali, proprieta, spettri e irraggiamento; pagina 9 per campioni
  effettivamente ricevuti. Valori tipici, non garantiti e non del lotto MINE.
- [Nagorna, risposte Kuraray](nagorna_kuraray_answers.pptx): slide 3 per curvature,
  slide 4 per spettri e tabella WLS, slide 5 per cladding, slide 6 per la proposta
  YS-4. Alcune immagini non hanno tutte le condizioni del test.
- PDF p.7: spettri di assorbimento/emissione in soluzione di monomero di stirene;
  tempi misurati su lastra PS con colorante; lunghezze di attenuazione su fibra
  da 1 mm, Non-S, multi-cladding. Questi osservabili non sono tutti una misura
  dello stesso oggetto fisico nello stesso assetto.

## Profili WLS che possiamo aggiungere/confrontare

| Fibra | Picco assorbimento [nm] | Picco emissione [nm] | Decay [ns] | Attenuazione dichiarata [m] |
|---|---:|---:|---:|---:|
| Y-11(200) | 430 | 476 | 6.9 | >3.5 |
| YS-1 | 395 | 454 | 2.7 | >3.5 |
| YS-2 | 422 | 474 | 3.2 | >3.5 |
| YS-4 | 420 | 470 | 1.4 | >3.0 |
| YS-6 | 414 | 462 | 1.3 | >3.0 |

Fonte: PDF pp.6-7, PPTX slide 4/6. I dati confermano il decay Y-11 gia attivo.
YS-4 e suggerito nelle risposte; nel PDF p.9 i campioni ricevuti sono invece
YS-2, multi-clad, Non-S, 1 mm, 2 x 10 m. Raccomandazione, campione e materiale
attualmente simulato Y-11 single-clad vanno mantenuti distinti.

Possiamo digitalizzare emissioni e forme relative di assorbimento di tutte le
serie. La comparazione spettrale copre circa 300-700 nm e puo estendere/verificare
il tratto UV della curva Y-11 corrente. Le curve di assorbimento sono disegnate
sotto lo zero per leggibilita: il segno e grafico. Mancano coefficienti assoluti
per le YS, concentrazioni/condizioni complete e misure equivalenti sul core
solido. Il coefficiente 0.0147/(ppm mm) di Herzkamp usato per Y-11 non va
trasferito automaticamente a un colorante YS diverso. Gli spettri in soluzione
non sono necessariamente identici a quelli nella fibra finita.

La slide 4 contiene anche diverse curve Y-11 eccitate a 430 nm senza distanze
identificate o ordinata quantitativa: da quelle sole non ricaviamo una
lunghezza assoluta di assorbimento o un quantum yield.

## Perdite nella fibra: priorita piu alta dopo il run lento

1. **Curva spettrale di transmission loss**, PDF p.7: ordinate in **dB/km**,
   quattro curve YS-1, YS-2, YS-4, YS-6. **Y-11 non compare nella legenda di
   questo grafico**. Le curve sono digitalizzabili e forniscono un vincolo
   quantitativo molto piu utile dell'ipotesi corrente di fibra senza perdite.
2. Conversione dell'intensita lungo la coordinata di misura z:
   T(z)=10^[-A(lambda) z_m/(10*1000)] = exp[-z_m/L_z(lambda)].
   L_z[m]=10000/[ln(10)*A(dB/km)]=4342.944819/A.
   Esempi di sola conversione: 100 dB/km -> 43.43 m, 500 -> 8.69 m,
   1000 -> 4.34 m. Non sono punti digitalizzati delle curve.
3. PDF p.8: anche Y-11 ha un vincolo numerico nella curva di attenuazione a
   dose zero. Lettura visiva approssimata: single-clad circa **3.8 m**,
   multi-clad circa **4.7 m**. Le ordinate del grafico sono in cm. Sono punti
   di campioni/test del produttore, non misure certificate del lotto MINE;
   l'incertezza di digitalizzazione e le condizioni mancanti vanno conservate.
4. Il limite tabellare >3.5 m da solo non determina una lunghezza esatta,
   una curva in lambda o due assorbimenti separati per PS e PMMA.

**Applicazione in Geant4:** questi dati sono vincoli di trasmissione lungo la
fibra. ABSLENGTH agisce invece sulla lunghezza effettiva s del raggio, maggiore
dell'avanzamento assiale. Inoltre la transmission loss puo comprendere
riassorbimento WLS, scattering e perdite di modi/superfici. Aggiungerla tutta
come assorbimento irreversibile sopra WLS e perdite geometriche gia tracciate
rischia un doppio conteggio. Mancano separazione delle componenti e dettagli
sull'eccitazione dei modi.

Proposta: calibrare prima una fibra diritta di riferimento contro il dato
assiale, dichiarando un'eventuale perdita efficace come tale; poi validare
bande spettrali e curvature. Per l'attuale Y-11 possiamo vincolare la lunghezza
assiale a dose zero, ma la curva spettrale Y-11 e i coefficienti indipendenti di
core e cladding vanno richiesti. L'analisi precedente resta soggetta alla
baseline senza assorbimento ordinario.

## Curvature: dati quantitativi e convenzione del diametro

PPTX slide 3 fornisce bending loss(%) vs **diametro di curvatura D**, separata
per S e Non-S. La tabella riguarda la sicurezza meccanica/affidabilita nel tempo:

| Diametro fibra | D minimo S | R minimo S | D minimo Non-S | R minimo Non-S |
|---|---:|---:|---:|---:|
| 2 mm | 200 mm | 100 mm | 400 mm | 200 mm |
| 1 mm | 100 mm | 50 mm | 200 mm | 100 mm |
| 0.5 mm | 50 mm | 25 mm | 100 mm | 50 mm |

Il nostro limite richiesto R>=100 mm per fibra da 1 mm e coerente con Non-S;
gli archi reali della baseline sono R=101 mm. Il grafico puo servire a validare
il trasporto nei raggi stretti o calibrare perdite residue legate a deformazione
meccanica. Non e automaticamente una probabilita per riflessione o per curva:
servono grade, diametro del campione, angolo/numero di pieghe e protocollo.
GEMC gia traccia la perdita geometrica da curvatura tramite le interfacce;
un fattore di bending aggiuntivo richiede evitare di contarla due volte.

## Single e multi-cladding

PDF p.5: core PS, cladding PMMA, ulteriore cladding FP per multi-clad.
Single round: NA=0.55 e trapping=3.1%; multi round: NA=0.72 e trapping=5.4%,
incremento teorico di luce circa 70%. Square single: trapping=4.2%, con una
geometria diversa da quella attuale.

Questi numeri sono riferimenti del cono guidato di una emissione nella fibra,
non l'efficienza complessiva di raccolta dalla lastra. Il confronto per un
singolo verso e coerente con f=(1-n_clad/n_core)/2: con i nostri n=1.59/1.49
si ottiene 3.145% e NA=sqrt(n_core^2-n_clad^2)=0.555. La somma dei due coni e
circa 6.29%. Questo e un controllo teorico con le sue ipotesi, non una misura
che includa ogni raggio skew, posizione di emissione e condizione di contatto.
Non si deve moltiplicare questo trapping sopra gli arrivi Geant4 gia tracciati.

PPTX slide 5: il disegno e esplicitamente **multi-cladding**, con Ti=2% D,
To=2% D e totale radiale T=4% D. Per D=1 mm: 20 micrometri PMMA + 20 FP,
core di diametro 0.92 mm. La frase testuale generica 'cladding 4% of diameter'
non basta a modificare lo strato singolo corrente da 20 a 40 micrometri.
Occorre confermare lo spessore single-clad del campione scelto.

Possiamo aggiungere un vero secondo strato FP e confrontare single/multi.
L'indice FP circa 1.42 si puo soltanto **derivare come riferimento** da NA=0.72
assumendo n_core=1.59; non e una curva n(lambda) misurata nelle slide. Gli indici
1.59/1.49 esistenti sono compatibili con NA single, ma la dispersione non e
fornita. La domanda n(lambda) compare nelle richieste e resta senza tabella.

## Irraggiamento e altri dati

PDF p.8: dati a 0, 5 e 20 kGy per attenuazione e luce letta a 100/285 cm.
Possiamo costruire scenari di degradazione con dose dichiarata. Y-11 single e
multi, YS-1/2 multi e YS-4/6 single sono costruzioni diverse: non si isola il
solo effetto del colorante confrontandole. Le ordinate di luce sono in mV,
non fotoni/MeV e non quantum yield; servono condizioni del fotodetettore,
accoppiamento, eccitazione, tempi di misura e recupero dopo irraggiamento.

PDF p.6: Clear-PS con attenuazione >10 m, dipendente da lambda. E un vincolo
utile per l'opzione storica clear, ma manca una sua curva spettrale nel documento.

## Proprieta che restano mancanti

- Perdite assolute spettrali distinte di core/cladding e scattering; spettro di
  transmission loss Y-11 e definizione dettagliata della misura.
- Quantum yield WLS assoluto, eventuale dipendenza da lambda e cinetica oltre
  al solo decay tipico: rise time o piu componenti non sono quantificati.
- Dispersione n(lambda) PS/PMMA/FP, quindi group velocity realistica di questi
  materiali oltre al riferimento costante gia usato.
- Spessore single-cladding, tolleranze geometriche, rugosita/microbending e
  perdite reali del contatto col gel.
- Dati del **plastico del pannello**: resa, spettro MIP, Birks, rise/decay,
  attenuazione vs lambda e Rayleigh. Le SCSF sono fibre scintillanti, non il
  materiale estruso della lastra MINE: i loro 2.4/2.8 ns non si trasferiscono.
- Riflettanza del rivestimento del pannello e caratteristiche del SiPM.

## Ordine utile dei prossimi interventi

1. Vincolare perdite finite delle fibre e aggiungere diagnostica prima di
   overflow patologici, distinguendo un errore numerico da una perdita fisica.
2. Profili opzionali YS-4/YS-6 e YS-2, con spettri e decay propri; calibrazione
   distinta dell'assorbimento assoluto. Il ramo GEMC esplicito oggi accetta Y-11.
3. Variante multi-cladding e verifica della raccolta nel gel reale.
4. Benchmark di curvature e, se rilevante per MINE, scenari di irraggiamento.

Il minor decay WLS e promettente per il timing, ma non implica lo stesso fattore
di miglioramento nella risoluzione totale: cambiano raccolta, spettro, propagazione
e statistica dei primi fotoni. I valori sono del produttore con le condizioni
indicate; nessuna validazione sperimentale MINE e stata dedotta da queste slide.
