# Ricerca sulla lunghezza di scattering Rayleigh

12 novembre 2026. **Solo ricerca/documentazione: nessuna nuova proprietà RAYLEIGH installata e nessuna modifica ai processi della simulazione.** Il Rayleigh rimane disabilitato nel ramo MINE esplicito (`source/physics/PhysicsList.cc`).

La grandezza cercata è la lunghezza libera media L_R(lambda), non una lunghezza d'onda di riemissione. Il processo Rayleigh è elastico; l'energia del fotone resta invariata. Si distingue da assorbimento, fluorescenza WLS e attenuazione tecnica di una barra/fibra. Riferimento per le definizioni: [Geant4 Physics Reference Manual](https://geant4.web.cern.ch/documentation/pipelines/master/prm_html/PhysicsReferenceManual/electromagnetic/optical_photons/optical.html).

## S21: fibre in polistirene, CERN / Kuraray

C. D'Ambrosio, H. Leutz, M. Taufer, T. Shimizu, O. Shinji, J. Sun, **Reflection losses in polystyrene fibres**, NIMA306 (1991)549–556, [DOI](https://doi.org/10.1016/0168-9002(91)90051-Q), [abstract dell'editore](https://www.sciencedirect.com/science/article/abs/pii/016890029190051Q), [preprint CERN/PPE91-51](https://cds.cern.ch/record/218076/files/cer-000130513.pdf).

L'abstract riporta L_R=3.6 m a 420 nm e 9.5 m a 530 nm, attribuendo le perdite a scattering su variazioni submicrometriche di densità del PS e separandole dalle perdite all'interfaccia core/cladding. Lo studio riguarda fibre di diametro 30 µm: **non sono misure sul pannello estruso FNAL né sul lotto Y-11 MINE**. I valori numerici sono verificati nell'abstract indicizzato di editore/preprint; il download integrale CERN è protetto da una pagina anti-bot, pertanto non si dichiara una rilettura completa di metodi/incertezze del PDF.

Una parametrizzazione provvisoria derivata, non una curva calibrata del pannello, è:

    L_R(lambda) = 3.6 m * (lambda / 420 nm)^4

Predice 9.13 m a 530 nm, vicino al 9.5 m riportato: la legge lambda^4 è approssimata, non un fit esatto di entrambi i punti.

## S22: PS purificato per fibre ottiche a bassa perdita

T. Kaino, M. Fujiki, S. Nara, **Low-loss polystyrene core-optical fibers**, J.Appl.Phys.52 (1981)7061–7063, [DOI](https://doi.org/10.1063/1.328702), [testo del PDF indicizzato presso LBL](https://www-eng.lbl.gov/~shuman/NEXT/CURRENT_DESIGN/TP/FO/POF_PS_low_loss_81.pdf).

Il testo alle pp.7062–7063 riporta scattering misurato prima della filatura: 55 dB/km a 633 nm, e la relazione esplicita:

    alpha_R(lambda) = 55 * (633 nm / lambda)^4  [dB/km]

La misura usa un'ampolla otticamente lucidata del PS polimerizzato e un laser He–Ne, con riferimenti benzene/tetracloruro di carbonio. Preparazione con purificazione, vuoto ed esclusione delle contaminazioni descritta nel lavoro: non rappresenta automaticamente un estruso commerciale drogato. Consultato il testo completo reso dal servizio web; il download diretto del mirror oggi restituisce 404 e gli screenshot non sono disponibili, quindi non viene dichiarata una nuova digitalizzazione della figura/tabella.

Conversione fra perdita di potenza in dB/km e coefficiente esponenziale:

    mu_R [m^-1] = ln(10)/10 * alpha_R[dB/km] / 1000
    L_R [m] = 1/mu_R = 4342.944819 / alpha_R[dB/km]
    L_R(lambda) = 78.962633 m * (lambda / 633 nm)^4

La conversione riguarda la componente di scattering separata, non la perdita totale della guida o quella per riflessione alla superficie.

## Confronto delle parametrizzazioni derivate

| lambda [nm] | S21 normalizzata a 420 nm, L_R[m] | S22 normalizzata a 633 nm, L_R[m] |
|---|---:|---:|
|420|3.60|15.30|
|425|3.77|16.05|
|476|5.94|25.25|
|530|9.13|38.81|

Tutti i valori della tabella sono calcolati dalle due formule. In particolare 9.13 m è l'approssimazione S21, mentre 9.5 m è il secondo dato effettivamente riportato nell'abstract. La differenza fra fonti non è una barra d'errore sullo stesso materiale: sono campioni e condizioni differenti.

Non è stata stabilita una curva assoluta di Rayleigh per l'esatto estruso FNAL scelto o per EJ-200. L'indice di rifrazione da solo non fissa la normalizzazione delle fluttuazioni del mezzo. Una legge lambda^4 può essere un modello di sensibilità, specialmente nel visibile lontano da bande/dispersioni forti; non va estrapolata senza riserve all'intera banda UV della fluorescenza FNAL.

## S23: scattering aggiuntivo nei plastici commerciali

N.R.Myllenbeck, S.Payne, P.L.Feng, **Environmental conditions leading to fogging in commercial poly(vinyltoluene) plastic scintillators**, NIMA954(2020)161782, [DOI](https://doi.org/10.1016/j.nima.2019.01.009), [articolo/abstract](https://www.sciencedirect.com/science/article/abs/pii/S0168900219300361).

Contesto, nessun numero importato: umidità/storia termica possono produrre difetti diffondenti nel PVT. Scattering da difetti/bolle/eterogeneità non è necessariamente Rayleigh e non è rappresentato automaticamente dalla legge lambda^-4 del polimero omogeneo.

## Conseguenze per MINE

Per un cammino già fissato s, a lambda costante, senza altre interazioni: P(almeno uno scattering)=1-exp(-s/L_R). A 425 nm e s=1 m le due ipotesi danno circa 23% e 6%. È una probabilità di scattering, non di perdita del fotone: direzione, raccolta e tempo vanno poi tracciati. Lo scattering non introduce un decadimento di fluorescenza, ma può allungare o modificare i cammini.

Se si aggiungerà Rayleigh, serviranno proprietà per materiale e controllo del possibile doppio conteggio con `bulkAbsorptionLengthMm=1000`: questo valore è attualmente un'ipotesi efficace di perdita, non una separazione sperimentale fra assorbimento e scattering. Le attenuazioni tecniche delle barre non vanno usate come L_R.

Input utente aggiornato: la coppia riassorbimento/riemissione radiativa nel plastico è considerata trascurabile per ora. Non viene inferita una richiesta di azzerare l'assorbimento bulk non radiativo già attivo o di modificare la WLS nella fibra.
