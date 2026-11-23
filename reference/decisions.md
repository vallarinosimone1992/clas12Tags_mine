# Proprietà, decisioni e limiti

Stato al 12 novembre 2026. Fonti Sxx in [sources.md](sources.md), input U01 in [user_inputs.md](user_inputs.md). Valori precisi dei singoli profili e parametri TEXT in [snapshot.json](snapshot.json). I percorsi sono relativi alla radice del repository.

## Scintillatore

| Proprietà / scelta | Provenienza e motivazione | Posizione / attività / limite |
|---|---|---|
| Birks 0.126 mm/MeV | Ereditato S19, accettato U01; quenching dell'energia depositata | `mine_parameters.h`, `mine__parameters_*.txt`; esplicito `birksMmPerMeV` in `geant4_optics*.conf`, proprietà Geant4 in `mine_geant4_optics.cc`. Non calibrato sul lotto MINE. |
| Resa 8000 fotoni/MeV, decadimento 2.5 ns | Input U01 per estruso | `lightYieldPerMeV`, `scintillationDecayNs`; controparti `mine_light_yield`, `mine_decay_time`. Emissione isotropa Geant4, ritardo esponenziale; `RESOLUTIONSCALE=1`, statistica nativa di scintillazione. Nessuna rise time separata nei profili estrusi storici; mini EJ-200 introduce rise 0.9 ns, come descritto sotto. Nessuna seconda componente lenta. |
| Emissione mono 425 nm | Ipotesi storica conservata nel default esplicito per confronto | `scintillationSpectrum=monochromatic`, `scintillationWavelengthNm`. Non è lo spettro reale dell'estruso. |
| Emissione `fnal_extruded` | Derivato S01: conserva entrambe le bande della fluorescenza UV | `mine_scintillation_spectrum.h`, dati `data/optics/scintillators`, profilo `geant4_optics_fnal_extruded.conf`. Trasferimento allo scintillatore eccitato da particelle non validato; non modellata la successiva autoassorbenza/riemissione PPO/POPOP nella plastica. |
| Emissione `ej200` | Derivato S02 | Profilo `geant4_optics_ej200.conf`; supporto del grafico fino a 500 nm e rinormalizzazione, coda esclusa. Cambia solo lo spettro, non rende il resto del modello un EJ-200 completo. |
| Densità spettrale | Ipotesi `photon_per_nm`; alternativa `power_per_nm` | Convenzioni nei config; conversione con Jacobiano da lunghezza d'onda a energia in `mine_scintillation_spectrum.h` e `mine_wls_spectrum.h`. L'ambiguità dell'ordinata originale rimane esplicita. |
| Bulk 1000 mm uniforme | Input efficace U01 trasferito come ipotesi di assorbimento lungo il cammino reale in esplicito | `bulkAbsorptionLengthMm`; nessuna curva assoluta affidabile stabilita. Trasmittanza S01 relativa a PS: `-ln(Trel)/10 mm` dà solo differenza di estinzione. S02 attenuazione tecnica e S16 assorbanza normalizzata non sostituiscono ABSLENGTH. |
| Indice plastica 1.58, costante | Ipotesi generica di modello, non misura dell'estruso acquistato | `nScintillator`; esplicito usa c/n (~190 mm/ns), nessuna dispersione. 170 mm/ns U01 rimane solo parametro efficace legacy. |
| Composizione materiale | Materiale generico ereditato | `geometry_source/mine/mine.pl`: `G4_PLASTIC_SC_VINYLTOLUENE`. Il profilo ottico FNAL non sostituisce il PVT con PS+PPO+POPOP nella fisica della particella carica. |

## Fibra e raccolta

| Proprietà / scelta | Provenienza e motivazione | Posizione / attività / limite |
|---|---|---|
| Y-11 single clad | U01, proprietà di riferimento S03–S05 | `fiberModel=Y-11`, modello `spectral_y11`. Concentrazione 200 ppm, diametro 1 mm sono scelte di riferimento da confermare. |
| Indici core/cladding 1.59/1.49; spessore radiale clad 2% del diametro | Documentato S04, trasferimento nominale | `nCore`, `nCladding`, `claddingFraction=.02`; per diametro 1 mm core R=.480 mm. Indici costanti, contatto ottico ideale senza colla/aria. Fresnel e riflessione totale scintillatore→clad→core nativi. |
| Assorbimento Y-11 spettrale | Derivato S03: `L=1/(0.0147*C*Arel(lambda)) mm` | `mine_wls_spectrum.h`, dati `data/optics/y11`; a 200 ppm L430=.340136 mm e L425=.387812 mm. Fuori supporto misurato assorbimento posto nullo, non coda inventata. Incertezza della didascalia S03 conservata. |
| Emissione Y-11 spettrale, decadimento 6.9 ns | Derivato S03 e documentato S05 | Config `wlsEmissionConvention`, `wlsDecayNs`; distribuzione energetica nativa e ritardo esponenziale WLS. QY=1 è **ipotesi non calibrata**, non dedotta dai soli spettri. |
| Perdita bulk core/clad ed efficace lungo fibra disabilitate | Ipotesi per isolare i processi conosciuti | `coreAbsorptionLengthMm=0`, `claddingAbsorptionLengthMm=0`, `effectiveFiberAttenuationMm=0`. WLS e perdite alle interfacce rimangono. Non significa fibra reale priva di attenuazione. |
| Attenuazione assiale 3500 mm e velocità efficace 182.56 mm/ns | Legacy: S04 limite >3.5 m usato come proxy; velocità derivata da indici e cono guidato | `FiberParameters` e `mine_fiber_*`; `v_eff=(c/ncore)*(1-r)/ln(1/r)`. Non aggiunti al trasporto esplicito: cammini obliqui e riflessioni sono già tracciati. Attenuazione tecnica non è lunghezza microscopica lungo il raggio. |
| Trapping 0.031 per verso | S04: condizionato a emissione isotropa nel core | Solo legacy `mine_wls_trapping_efficiency`. Il 6.2% totale non è l'efficienza di raccolta di un pannello. In tabulato/esplicito non viene rimoltiplicato. |
| Collection efficiency 0.01 | Ipotesi legacy non calibrata | `CollectionParameters.efficiency`, `mine_collection_efficiency`. Non usata per ricalibrare il trasporto esplicito o le tabelle complete. |
| Coupling 1 | Input U01 | `mine_coupling_efficiency`; neutralità del fattore, nessuna certificazione di accoppiamento reale del fascio sul SiPM. |
| YS-6 462 nm, 3000 mm, 1.47 ns | Profilo futuro legacy: S04/S06 + ipotesi di trasferimento single clad | `mine_fiber_model=1` in `mine_parameters.cc`. Storico: esplicito YS-6 inizialmente rifiutato; dal 20 novembre implementato con decay nominale 1.3 ns e spettri propri, senza cambiare questo profilo legacy. Vedi [WLS](wls_fibers.md). |
| Vecchio WLS monocromatico 476 nm, assorbimento .34 mm | Storico, ordine di grandezza S03/S15 | `geant4_optics_monochromatic.conf`, `GEANT4_WLS_SOURCES.md`; non assorbimento indipendente dalla lunghezza d'onda validato. |

## Rivestimento esterno

| Materiale | Provenienza, legge e scelta | Limiti |
|---|---|---|
| `fnal_tio2` | S08/S09, array grezzi e interpolazione lineare in energia; diffusione Lambertiana efficace, R425≈.873 | Nessun tuning empirico Mu2e trasferito. Plateau UV e calibrazione originale incompleta dichiarati. Ricetta non universale; legge angolare da verificare alle incidenze radenti. |
| `tyvek` | S10, Tyvek 150 µm, curva digitalizzata; `coatingModel=diffuse`, R425≈.967 relativo a BaSO₄ | Lambertiano è approssimazione (S11), non BRDF esatto. R assoluta al contatto col pannello non misurata. |
| `aluminized_mylar` | S10, Al-Mylar 10 µm, speculare; R425≈.747 relativo a BaSO₄; attuale default dei config espliciti principali | Specifico campione, non tutti i film commerciali. Il precedente 98% non è supportato per questo campione. |
| `constant` | Storico/ipotesi; R=.98 speculare o R=.92 BC-621 diffuso (S13) | Conservato nei profili storici e nei diagnostici. BC-621 non viene presentato come capstock FNAL. |

Implementazione: `mine_coating_spectrum.h`, `mine_coating_spectral_data.h`, `mine_geant4_optics.cc`; CSV e provenienza in `experiments/mine/data/optics/coatings`. Per Tyvek/Mylar `R=Rrel*coatingReferenceAlbedo`, default 1: riferimento ideale esplicito, non calibrazione assoluta. Interpolazione Yang lineare in lambda, griglia nativa 0.5 nm; probabilità senza Jacobiano. Fuori supporto estremi costanti: ipotesi irrisolta. Nessuno shift dei picchi.

Superficie opaca UNIFIED: `dielectric_metal` è la definizione efficace di riflessione/perdita, non composizione metallica di Tyvek/TiO₂. Mirror `polished` o `ground` con componenti speculari nulle per Lambert; solo bordo esterno pannello/cladding→ambiente. Nessuna superficie riflettente interna core/cladding, nessun fattore Fresnel esterno aggiuntivo. Fotone riflesso conserva energia e non ha ritardo di fluorescenza; il cambio di percorso modifica il tempo di arrivo. Non è un assorbimento seguito da nuova emissione WLS.

## Geometria, lettura e tempi

| Scelta | Provenienza / giustificazione | Posizione e limiti |
|---|---|---|
| Corner fan 15 fibre, pannello 1×1 m×1 cm | U01/S20; estensione geometrica reale con cilindri e settori toroidali | `mine_optical_geometry_model.cc`, `mine_geant4_optics.cc`, `geometry_source/mine`. Corner fan storico; dal 23 novembre è supportato anche il mini-panel a spirale descritto sotto. |
| Pitch nominale 100 mm, Rmin 100 mm | Geometria S20 e riferimento non-S 1 mm S04 | Auto-fit rende pitch 94.112143 mm, raggio di raccordo 101 mm, inset 10.268989 mm e readout largo 20.501215 mm nel profilo 15. Numeri derivati, non misure. |
| Readout 20×1.5 mm² nominale | U01 | Assorbitore numerico spesso 0.01 mm, ritagliato nel pannello; non finestra reale SiPM. Nella configurazione nominale altezza effettiva 1.25 mm. Tutti gli ingressi sono contati; `entryRole=1` seleziona il core per confronto col MC che conta solo questo. |
| Coordinate Z | Convenzione derivata S20 | `z_local=z_MC-thickness/2`; zMc=.5 mm dà local −4.5 mm nel pannello 10 mm. Offset per fibra espliciti, validati. |
| Weld geometrico 1e−4 mm | Soluzione numerica, non gap fisico | Giunzioni Boolean fra cilindro e toro; bound controllato rispetto alla tolleranza Geant4, dettagli in `GEANT4_OPTICS.md`. |
| Velocità lungo ogni cammino | S18 e indici del config | Geant4 c/n per indici costanti; sincronizzazione dopo boundary a passo nullo in 10.7.4 è correzione tecnica, non aggiustamento fisico della velocità. |
| Processi ottici | S18 + scelta di modello | `source/physics/PhysicsList.cc`: scintillazione, assorbimento, boundary, WLS; Cherenkov, Rayleigh e Mie disabilitati in modalità MINE esplicita. |
| Tempi tabulati vs espliciti | U01, evitare doppio conteggio | `mine_optical_response.*`, `mine_response.cc`, contratto documentato in README. Metadati della tabella stabiliscono quali ritardi sono inclusi. Esplicito usa direttamente gli arrivi e non aggiunge un secondo decadimento/ritardo o una seconda estrazione del numero di fotoni. |

## SiPM, elettronica e analisi

| Proprietà | Provenienza / scelta | Posizione / attività / limite |
|---|---|---|
| PDE(lambda), 1600 celle, dark 500 kcps, cross-talk.07 | S07 documentato/digitalizzato | `mine_sipm_pde.h`, `mine_parameters.h`, `mine_response.cc`; curva270–900 nm approssimata, fill factor già incluso. Temperatura/sovratensione non dinamiche. Modelli di cross-talk e occupazione semplificati, senza recupero delle celle. |
| Gain1 | U01 normalizzazione | Non il gain fisico 4×10⁶ elettroni. Afterpulse 0 è ipotesi perché non quantificato dalla fonte; tempo 20 ns inattivo è non calibrato. Override PDE costante 0.25 è ipotesi, non valore integrato del datasheet. |
| SiPM selezionabile nel ramo esplicito | Default ideale richiesto per il mini-panel; la precedente imposizione ideale è storica | `sipmModel=ideal|hamamatsu` nel config ottico prevale sul TEXT. Ideale: PDE100%, rumori/cross-talk/saturazione disattivati. Hamamatsu: un solo test PDE per arrivo tracciato, celle finite e rumori semplificati; nessun jitter aggiuntivo. |
| Impulso differenza di esponenziali rise 1/fall 5 ns, campionamento 0.25 ns | Parametri ereditati, mantenuti U01 | `mine_waveform.cc`, `ElectronicsParameters`; gate −5..100 ns, gain ADC 1, soglia 0.05 carica/ns, rumore 0, bin TDC 0 con crossing interpolato. Nessuna risposta di elettronica misurata. Il gate digitale non impone un taglio ai cammini ottici: fotoni tardivi possono esistere fuori gate. |
| Jitter aggiuntivo 0; eventuale somma di varianze | U01, modello analitico in analisi | Progetto esterno `simulation_time_resolution/config`; `Jitter.Sensor0Sigma_ns`, `Sensor1Sigma_ns`, correlazione, comune e combinato. Contributi indipendenti possono sommarsi in quadratura; correlati richiedono covarianza e dipendono dall'estimatore. Non simula automaticamente il jitter di ogni fotoelettrone. |

## Lacune da conservare visibili

Mancano la caratterizzazione del lotto reale (emissione da particella, autoassorbimento/riemissione plastica, assorbimento assoluto spettrale, dispersione), perdite intrinseche reali della fibra e resa quantica calibrata, BRDF/riflettanza assoluta del rivestimento nel contatto reale, accoppiamento fisico fascio/SiPM, calibrazione del modello SiPM semplificato e dell'elettronica. Il superamento dei test numerici non elimina queste assunzioni.

## Aggiornamento: groove e verità WLS

I preset espliciti correnti adottano ora groove aperti 2×2 mm, fibra centrata a 1 mm dalla superficie e gel G608N3, con alternative aria e colla EJ-500. Questo supera il precedente contatto diretto plastico/cladding e zMc=.5 mm nei preset correnti; i file storici senza groove conservano il vecchio modello. Indici, proxy chimici, interpolazione, copertura esterna e output `MC::WLS` sono documentati in [grooves.md](grooves.md). Sono input utente le dimensioni, il centraggio e l’assenza di assorbimento nel riempimento; le marche di gel/colla sono riferimenti implementativi con fonti S24–S26.

## Aggiornamento WLS del 20 novembre 2026

Perdita ordinaria comune 4000 mm nel trasporto esplicito, YS-6/YS-4 disponibili;
fonti, ipotesi, stato attivo e verifiche in [wls_fibers.md](wls_fibers.md).
Le precedenti righe storiche con assorbimento disabilitato descrivono la baseline archiviata.

## 22 novembre 2026: sostituzione dei 4 m con le curve spettrali

`fiberLossModel=kuraray_spectral`, vecchi scalari tutti zero; stesso assorbimento
ordinario core/cladding, coda WLS nel solo core ricavata come parte del budget,
proxy Y-11 media dei coefficienti YS-4/YS-6. Ipotesi 480/520 nm e limiti
in [wls_spectral_loss.md](wls_spectral_loss.md). Benchmark monocromatico superato;
attenuazione integrale circa 10-11 m, non forzata ai valori nominali 3-4 m.

## 23 novembre 2026: mini-panel a spirale

`geometry=mini_panel_spiral` e variation TEXT omonima realizzano il pannello
300×500×8 mm con due fibre sovrapposte Y-11 Non-S, cladding radiale 3% del diametro,
groove 1×2 mm in gel, S sollevata sotto Mylar con aria e rampe a 30°, due SiPM
3×3 mm e gap gel 1 mm. Il readout accetta anche luce diretta dal pannello.
Geometria sorgente, approssimazioni numeriche e controlli sono descritti in
[mini_panel_spiral.md](mini_panel_spiral.md). Il launcher rifiuta questa variation
in modalità `tabulated` e seleziona il gcard con generazione uniforme 300×500 mm.

Il preset EJ-200 applica i valori nominali Eljen: resa 10000 fotoni/MeV,
rise/decay 0.9/2.1 ns, densità 1.023 g/cm³, indice 1.58 e spettro digitalizzato.
La rise time attiva la legge nativa Geant4; il default zero mantiene i profili
storici. Bulk 1000 mm e Birks 0.126 mm/MeV restano assunzioni provvisorie.
Fonti, distinzione fra attenuazione tecnica e microscopica e test di generazione
in [mini_panel_materials.md](mini_panel_materials.md).

Il preset ideale e quello `sipmModel=hamamatsu` condividono geometria e trasporto;
il secondo applica PDE spettrale e modello SiPM semplificato una sola volta
agli arrivi reali. Restano non calibrati il recupero delle celle e SPTR; jitter
aggiuntivo zero. Il raggio minimo 30 mm non è una validazione meccanica della
fibra Non-S, e QY=1, assorbimento del gel trascurabile e riflettanza Mylar
relativa rimangono ipotesi esplicite.
