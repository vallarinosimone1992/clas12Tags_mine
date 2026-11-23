# Perdite spettrali Kuraray e separazione del WLS

Modello corrente dal **22 novembre 2026**, solo trasporto esplicito Geant4.
Profili distinti Y-11, YS-6 e YS-4, single-cladding. Sostituisce il
[modello storico a 4 m](wls_fibers.md); le vecchie validazioni e i precedenti
risultati HIPO/analisi rimangono conservati, senza reinterpretazione retroattiva.

## Fonte, decisione e proprieta comuni

**Documentato:** Kuraray, presentazione del meeting 13 novembre 2026 p.7,
*Transmission loss*, quattro curve YS; qui usate YS-4 e YS-6. Originale e SHA256
in [manufacturers/kuraray_20261113](manufacturers/kuraray_20261113/assessment.md).
Gli spettri sono digitalizzati da raster con risoluzione limitata; le curve
sono valori tipici, non misure sul lotto MINE. Le perdite spettrali non sono
coefficienti separati di PS e PMMA, e il protocollo completo di modi/banda
spettrale non e riportato.

**Input utente:** usare le curve in funzione di lambda, uguali per core e
cladding, evitare doppio conteggio del WLS, mantenere i tre profili, verificare
una fibra diritta e documentare una sostitutiva Y-11.

**Interpretazione adottata:** la transmission loss descrive l'estinzione del
fascio monocromatico iniziale, non il conteggio cumulativo di fotoni dopo
riemissione WLS. Si assume una misura circa assiale, s~z per quel riferimento.
Geant4 continua a usare il cammino reale per tutti gli altri angoli. Questa
interpretazione e un'ipotesi, non una ricostruzione certificata del setup Kuraray.

La **stessa ABSLENGTH ordinaria** viene installata nel core e nel cladding.
Solo il core contiene WLS. Gli indici rimangono distinti (PS 1.59, PMMA 1.49);
non viene uguagliata la rifrazione dei due materiali. Geometria invariata:
diametro 1 mm, PMMA radiale 0.02D, core 0.96 mm, pannello/groove/readout invariati.

## Curva sostitutiva Y-11

La presentazione non include una curva di transmission loss Y-11. Si usa:

    A_Y11(lambda) = [A_YS4(lambda) + A_YS6(lambda)] / 2  [dB/km]

E la media aritmetica dei **coefficienti**, non delle lunghezze e non degli
spettri di emissione. Pesi 1/2 scelti per non privilegiare arbitrariamente una
serie; sono un'ipotesi, non una misura Y-11. Si interpola ciascuna curva in
lambda prima della media, nel dominio comune **497.176-699.502 nm**.
YS-4 e risolta da 497.176 nm, YS-6 da 486.213 nm; a lambda piu blu la curva
supera il bordo del grafico o non e risolta. Nessun punto sopra il limite di
2000 dB/km e presentato come digitalizzato.

CSV derivato: [y11_transmission_proxy.csv](../experiments/mine/data/optics/kuraray_wls/y11_transmission_proxy.csv).
Generatore: `scripts/generate_mine_kuraray_transmission.py`; dataset, hash,
domini e convenzioni: [transmission_provenance.json](../experiments/mine/data/optics/kuraray_wls/transmission_provenance.json).
Y-11 conserva il proprio spettro di emissione e il proprio assorbimento blu
Herzkamp; non assume il colorante o il decadimento delle YS.

## Conversione e budget dei processi

    P_unshifted(z,lambda) = exp[-alpha_ref(lambda) z]
    alpha_ref[mm^-1] = ln(10) * A[dB/km] / 1e7
    L_ref[m] = 4342.944819 / A[dB/km]

Non si aggiunge A(lambda) interamente a una seconda curva WLS: si scompone
un solo budget. Definita lambda_h=520 nm:

    alpha_host(lambda) = alpha_ref(max(lambda,520 nm))
    alpha_WLS(lambda,200 ppm) = max(0,alpha_ref(lambda)-alpha_host(lambda))
          per lambda_min_misurata <= lambda < 520 nm
    alpha_WLS(lambda>=520 nm) = 0

Cosi, nel dominio misurato e alla concentrazione di riferimento:

    alpha_core_tot = alpha_host + alpha_WLS = alpha_ref
    alpha_cladding = alpha_host

**Solo alpha_host e uguale nei due materiali.** Il core ha in piu il processo
WLS previsto dalla fibra, contato una volta. La perdita spettrale e mantenuta
con tutti i picchi/bande nel rosso, incluso quello vicino a 610 nm, attribuiti
in questa scomposizione al fondo ordinario.

La separazione a 520 nm e un'**ipotesi fenomenologica**: negli spettri relativi
l'assorbimento del colorante e gia sotto risoluzione, mentre le curve di perdita
entrano nella regione rossa con bande comuni dei polimeri. Non e una soglia
chimica misurata. Definisce un fondo positivo e finito senza sottrarre tassi
negativi. Il fondo e tenuto al valore di 520 nm nel blu; oltre 699.502 nm si
tiene l'ultimo valore del grafico. Le estensioni UV/IR non sono misurate.

Il WLS risolto nel blu, **lambda<=480 nm**, resta:

    alpha_WLS_blue = .0147 * C_eff_ppm * Arel_profile(lambda)  [mm^-1]

La scala YS e ancora un trasferimento dichiarato da Y-11 (Herzkamp), non una
misura assoluta delle concentrazioni YS. Fra 480 nm e il primo punto di
transmission loss si raccorda **linearmente log(alpha_WLS)**, preservando la
continuita e senza imporre un andamento al grafico fuori dal suo dominio.
Questa e una sostituzione della coda debole prima non risolta, non una seconda
coda aggiunta al WLS vecchio.

Riferimento C_eff=200 ppm. Se si cambia concentrazione, il tasso WLS scala
con C/200, il fondo ordinario rimane invariato: l'uguaglianza con la curva
Kuraray e verificata al riferimento 200. Il quantum yield resta 1; decadimenti
6.9 ns Y-11, 1.3 ns YS-6, 1.4 ns YS-4. Ogni riassorbimento nativo conserva la
propria emissione spettrale, con ritardo esponenziale del profilo e restrizione
energetica Geant4. Non si applicano attenuazioni o ritardi aggiuntivi nell'hit process.

**Esempio:** a 550 nm non e previsto WLS. L ordinaria e circa 29.22 m per la
proxy Y-11, 22.99 m YS-6, 40.09 m YS-4. Nel vecchio modello tutte erano 4 m.
A 500 nm la curva totale ha invece L circa 4.57,10.13,2.95 m rispettivamente:
la differenza fra perdita totale e fondo e WLS, non assorbimento irreversibile.

## Configurazione, codice e protezioni

Nei config spettrali attivi, incluso il default:

    fiberLossModel=kuraray_spectral
    effectiveFiberAttenuationMm=0
    coreAbsorptionLengthMm=0
    claddingAbsorptionLengthMm=0

Gli zeri disabilitano i vecchi scalari: **non** disabilitano le curve spettrali.
Un qualsiasi scalare di fibra non nullo insieme al modello spettrale causa
un errore prima degli eventi, per evitare aggiunte duplicate. `fiberLossModel=none`
conserva i modelli storici scalari e la vecchia coda WLS senza questa correzione.
I config monochromatic/geometry-only rimangono storici.

- `mine_fiber_loss.h`: curva, media Y-11, conversione, fondo, coda e raccordo.
- `mine_kuraray_transmission_data.h`: copia compilata delle curve originali;
  identica a quella presso i CSV.
- `mine_geant4_optics.cc`: proprieta ABSLENGTH identiche, WLSABSLENGTH budgettata
  solo nel core, griglia in energia con passo 0.25 nm e vertici delle fonti.
- `mine_optical_geometry_model.{h,cc}`: opzione, controlli, hash. L'hash include
  tutti i vertici e le scelte 480/520 nm, proxy e regole di interpolazione.

Nessun file ottico esterno richiesto durante il run: le tabelle sono compilate.
Il SiPM ideale, la fisica della particella e le altre impostazioni non cambiano.
La modalita tabulata e separata e non riceve queste modifiche automaticamente.

## Verifica nativa su fibra diritta

Geant4 10.7.4; ambiente/compilatore nei log. **6000 fotoni per caso**, seed
20261122 per core, 20261123 per cladding, 20261124 per spettro completo.
Single-cladding 1 mm in aria; piani virtuali a 0.1,0.25,0.5,1,1.5,2,3,4,5,6 m.
Nessun taglio sul tempo del fotone. Il watchdog numerico provoca fallimento del
test in caso patologico, non rimuove silenziosamente fotoni dalla statistica.

**34 casi monocromatici:** core assiale a 500,505.125,510,520,550,600,610,650,690 nm
per ciascun profilo; core YS-6 anche a 490 nm; cladding assiale a 500/550 nm per
profilo. Il WLS resta attivo. Si contano separatamente il primo attraversamento
della primaria non riemessa e quello della linea completa di figli WLS.
Il confronto alla curva usa la **trasmissione non shiftata**; i figli vengono
comunque propagati. Nel cladding i conteggi sono identici perche non c'e WLS.

I materiali sono costruiti con gli stessi helper di GEMC. Verificate uguaglianza
core/cladding ordinari e somma dei tassi nel materiale nativo entro 0.2% di
interpolazione, anche a 505.125 nm fuori dalla griglia regolare. Il test portabile
verifica inoltre il budget ogni 0.125 nm, il mantenimento del blu risolto, la
media della proxy e il rifiuto dei vecchi scalari.

Tutti i 34 casi superano, a ogni piano:

    |P_native-P_ref| < 6 sqrt[P_ref(1-P_ref)/N] + 0.002

Questo e un controllo numerico della coerenza con gli input, non un errore
sperimentale assegnato a Kuraray. Scarto assoluto massimo osservato: 0.01310.
A titolo di controllo, fit della trasmissione a 550 nm (errori statistici):

| Profilo | L da curva [m] | L nativa [m] |
|---|---:|---:|
| Y-11 proxy | 29.22 | 27.45 +/- 0.88 |
| YS-6 | 22.99 | 22.85 +/- 0.68 |
| YS-4 | 40.09 | 39.11 +/- 1.48 |

## Diagnostica integrale: discrepanza da mantenere esplicita

Generazione sul core con spettro di emissione completo e direzioni isotrope
condizionate al cono meridionale guidato verso +z; tutti i riassorbimenti e tempi
WLS sono tracciati. Nessuna probabilita di trapping moltiplicata dopo il tracking.
Il fit della trasmissione completa 1-6 m e una diagnostica dipendente dalla
finestra, non una lunghezza intrinseca e non un target da imporre:

| Profilo | L efficace 1-6 m [m] | Devianza / ndf | Lettura indicativa p.8 [m] |
|---|---:|---:|---:|
| Y-11 | 11.36 +/- 0.35 | 39.5 / 5 | circa 3.8 |
| YS-6 | 9.74 +/- 0.30 | 19.2 / 5 | circa 3.8 |
| YS-4 | 10.48 +/- 0.34 | 66.9 / 5 | circa 3.4 |

Quindi il modello **riproduce le curve monocromatiche secondo l'interpretazione
adottata**, ma **non riproduce il valore integrale di circa 3-4 m** del grafico
Kuraray nelle condizioni del nostro test. Le miscele spettrali non sono neppure
esponenziali singole, come indicano le devianze. Gli errori in tabella sono
condizionati a quel fit, non sistematiche dei materiali.

Non e stata reintrodotta una perdita costante per nascondere la discrepanza.
Servono il protocollo del produttore (eccitazione, distanze/finestra di fit,
modi guidati, accettanza e risposta spettrale del sensore), la reale coda di
riassorbimento e/o misure del lotto per distinguere gli effetti. La separazione
host/WLS, il raccordo e la proxy Y-11 rimangono ipotesi da variare sistematicamente;
non sono dimostrati dalla sola uguaglianza del budget spettrale.

Gli smoke GEMC (un muone da 1 GeV per profilo, seed 20261122) producono HIPO
completi, con fotoni su entrambi i sensori e ID 0/1/2 corretti. Compilazione,
regressioni portabili e test nativi degli spettri sono superati.

Risultati: [JSON](validation/wls_spectral_20261122/spectral_loss_summary.json),
[confronto spettrale PNG](validation/wls_spectral_20261122/spectral_transmission_validation.png),
[diagnostica integrale PNG](validation/wls_spectral_20261122/broadband_transmission_diagnostic.png).
CSV, config, hash e riepiloghi HIPO nella medesima cartella. Log completi e HIPO
in `build/validation/wls_spectral_20261122`. Il primo tentativo di compilazione
del test richiedeva un adattamento const alla API Geant4 10.7.4; i dati finali
sono nella sottocartella `native`, non in `compile_attempt`.

## Riprodurre e usare

```sh
# Dalla radice del repository, nuovo output automatico.
scripts/test_mine_wls_spectral_loss.sh --photons 6000
# Anche il vecchio nome test_mine_wls_fibers.sh inoltra al test corrente.

# Profili pronti: sostituire ys4 con ys6 o y11.
bash scripts/run_mine.sh --batch --transport geant4 \
  --optical-config experiments/mine/geant4_optics_ys4.conf -N=1000

# Rigenerare header e proxy dai CSV originali; copiare poi l'header nel sorgente.
python3 scripts/generate_mine_kuraray_transmission.py
# Rigenerare i PNG delle reference.
python3 scripts/plot_mine_references.py
```

La compilazione GEMC e gia completata nel container. I vecchi risultati e i
loro metadati restano associati ai vecchi hash e alle vecchie ipotesi.
**Numerical validation is not experimental validation.**
