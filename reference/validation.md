# Verifiche della versione documentata

12 novembre 2026. Queste sono verifiche di implementazione e integrazione, non confronto con misure di MINE.

- Ricompilazione GEMC conclusa: [cartella di build](../experiments/mine/validation/wrap-spectral-build.zJ4NuF4N).
- Suite completa conclusa: [validation.log](../experiments/mine/validation/optics_BTrOHEmp/validation.log), ambiente Geant4 10.7.4 in [environment.txt](../experiments/mine/validation/optics_BTrOHEmp/environment.txt).
- Tre rivestimenti verificati con il costruttore Geant4 usato in produzione: **35 000 fotoni per materiale**, sette casi da 5000. Incidenze 0/45/75° a 425 nm, più 350/400/550/700 nm a 45°. Controllati probabilità spettrale, direzione speculare o distribuzione Lambertiana/azimutale, conservazione di lunghezza d'onda e tempo e assenza di secondari.
- Controlli portabili su array originali, assenza della correzione empirica Mu2e, interpolazione, calibrazione, configurazioni incompatibili, hash e uguaglianza delle copie degli header generati: superati.
- Regressioni spettri scintillazione/WLS, risposta tabulata ed esplicita e confronto fibra retta/curva: superati nella stessa suite.
- Integrazione **1 evento per ramo**: esplicito con `geant4_optics_tyvek.conf` e tabulato con la tabella attiva. HIPO prodotto e verificato; 58 campi della banca hit, metadati/hash e collegamenti ottici controllati. Nel test esplicito 201 arrivi (96/105 sui due sensori), lettura dei tempi fino a circa 818 ns; il SiPM forzato ideale conserva il numero di arrivi.
- Lettura HIPO ed esecuzione dell'analisi con clas12root in una copia privata del progetto: concluse. Un evento non stima una risoluzione temporale e non rappresenta statistica di produzione.

Ripetizione della suite (container GEMC avviato e binario già ricompilato):

```sh
bash scripts/test_mine_optics.sh --photons 2000 --integration 1 \
  --optical-config experiments/mine/geant4_optics_tyvek.conf
```

Per provare un evento senza GUI, dalla radice del repository:

```sh
scripts/run_mine.sh --batch --transport geant4 --geometry corner_fan \
  --optical-config experiments/mine/geant4_optics_diffuse.conf -N=1
```

`geant4_optics_diffuse.conf` seleziona TiO₂ FNAL; `geant4_optics_tyvek.conf` Tyvek; `geant4_optics.conf` Mylar alluminato. Questi profili mantengono la sorgente mono 425 nm: per studiare insieme la fluorescenza FNAL impostare `scintillationSpectrum=fnal_extruded` nel profilo scelto. Selezione spettro e materiale sono indipendenti.

Per la GUI sostituire `--batch` con `--gui` e omettere `-N=1`: si apre la GUI senza generare automaticamente eventi; impostare 1 e premere **Run**. HIPO e log rimangono in `experiments/mine/output` sull'host. Il trasporto completo di un evento può richiedere tempo. Lo script gestisce Ctrl+C per il proprio run; `scripts/stop_mine.sh --list` elenca i processi e lo stesso script senza `--list` li arresta.

## Groove aperti e identificazione WLS — 12 novembre 2026

Verifica finale con unioni bilanciate, successiva alla versione preliminare scartata per avvisi di navigazione. GEMC ricompilato; [build e prove aggiuntive](../experiments/mine/validation/grooves_b8vPaqUw), [suite finale](../experiments/mine/validation/optics_JnCg6EfF/validation.log).

- Controlli geometrici nativi superati per **gel, aria e colla**: sovrapposizioni assenti, 90 punti lungo le fibre, angoli quadrati, apertura a filo, indici e velocità di gruppo. Il gel richiede una tabella dispersiva nuova: riutilizzare la precedente GROUPVEL generata dall'indice costante conservava una velocità errata; il test ha rilevato il problema ed è stato corretto prima della versione finale.
- Eventi di integrazione: 1 con gel nella suite finale e 1 ciascuno per aria e colla nelle prove aggiuntive; output HIPO verificati e log privi di `G4Exception`, `GeomNav` e `GeomSolids`. Sono prove funzionali, non stime statistiche delle efficienze.
- Gel: 81/65 arrivi ai sensori, 1492 assorbimenti WLS; aria: 137/162 arrivi, 836 assorbimenti; colla: 45/48 arrivi, 1265 assorbimenti. Il controllo ricostruisce le catene dei parent nella banca `MC::WLS` e confronta prima/ultima fibra e numero di WLS con `MINE::optical`.
- `MINE::dgt` resta 58 campi; `MINE::optical` ora 15; nuova banca `MC::WLS` 11. Ramo tabulato verificato: nessuna riga ottica/WLS artificiale, hash della tabella invariato. Analisi clas12root eseguita con successo su copia privata del progetto.
- Regressioni di risposta, spettri scintillazione/WLS, rivestimenti e fibra retta/curva superate. Galleria delle reference aggiornata: 18 PNG, tutti i 13 CSV rappresentati, 17 copie identiche accanto ai dati.

Le cartelle preliminari `optics_hlTulrxI` (velocità di gruppo) e `optics_4dVxCnKM` (MultiUnion con normali non valide) **non** sono la validazione finale; i loro pseudo-dati non vanno usati per confronti fisici. L'hash identifica il config/modello, non certifica da solo la correttezza numerica del binario. La fotografia corrente include l'hash del binario compilato.

## 20 novembre 2026: tre profili WLS e perdita comune

GEMC ricompilato; regressioni portabili e risposta esplicita superate.
Test nativi degli spettri e benchmark completo della fibra diritta:
[metodo/risultati](wls_fibers.md), [JSON](validation/wls_20261120/attenuation_summary.json).
Tre smoke HIPO, un muone per profilo, verificati su entrambi i sensori e ID fibra.
La compatibilita riguarda l’attenuazione complessiva, non le curve spettrali
di transmission loss o la separazione microscopica PS/PMMA.

## 22 novembre 2026: trasmissione spettrale

GEMC compilato, test nativi dei materiali e 34 casi monocromatici superati.
Regressioni portabili e tre smoke HIPO verificati. La diagnostica integrale
rimane diversa dai valori nominali: [risultati e limiti](wls_spectral_loss.md),
[JSON](validation/wls_spectral_20261122/spectral_loss_summary.json).
Il controllo spettrale non certifica la scomposizione microscopica delle perdite.


## 23 novembre 2026: mini_panel_spiral nativo

GEMC ricompilato; configurazione EJ-200/Y-11 Non-S/Mylar/gel, SiPM ideale o
Hamamatsu. Modello portabile, 434 punti di navigazione, 3472 controlli di
contenimento e 100 prove ottiche di accoppiamento completati. 99/100 seguono
il percorso immediato; una riflessione Fresnel ritarda il fotone restante.
Due eventi completi (un muone per modalità) producono HIPO validati: 72/66
arrivi in entrambi, 72/66 PE ideali oppure 34/31 Hamamatsu. Nessuna eccezione
Geant4 di geometria/navigazione. Costo circa 126 s/evento centrale sul Docker
corrente. Regressioni della risposta e del groove corner-fan superate.

[Ipotesi e limiti](mini_panel_spiral.md),
[log,config,hash e risultati](validation/mini_panel_20261123/README.md).
L'overview esterna non è ancora adattata alla mini geometria e all'ingresso
dal gel; la GUI non è stata verificata interattivamente. Le prove sono
numeriche, non una validazione sperimentale.
