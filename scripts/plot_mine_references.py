#!/usr/bin/env python3
"""Render all local MINE spectral reference CSVs with consistent axes and PNG size.

Uses only existing local data. No changes to simulation parameters or spectra.
Default output: reference/plots, plus byte-identical PNGs beside the input CSVs.
Dependencies: numpy, matplotlib, Pillow (PNG verification).
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "mine-reference-mpl"))

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator, LogLocator, NullFormatter
import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[1]
DATA = REPO / "experiments/mine/data"
HC_EV_NM = 1239.841984332  # Same constant as mine_wls_spectrum.h.
SIZE = (1800, 1200)
DPI = 180
XLIM = (250, 900)
COLORS = {
    "fnal": "#2463A6", "ej200": "#C55C12", "y11": "#207849",
    "absorption": "#8043A4", "transmission": "#576679",
    "tio2": "#2463A6", "tyvek": "#207849", "mylar": "#C55C12",
    "bc621": "#8043A4", "pde": "#2463A6",
}
plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 12,
    "axes.labelsize": 14, "axes.labelcolor": "#202B38",
    "xtick.labelsize": 11, "ytick.labelsize": 11,
    "axes.edgecolor": "#83909F", "axes.linewidth": 0.8,
    "text.color": "#202B38", "legend.fontsize": 11,
    "lines.linewidth": 2.1, "savefig.facecolor": "white",
})


def load_csv(relative_path, x_key, y_key, relative=True):
    path = DATA / relative_path
    with path.open() as stream:
        rows = list(csv.DictReader(line for line in stream if not line.lstrip().startswith("#")))
    points = np.array([(float(r[x_key]), float(r[y_key])) for r in rows])
    if not len(points) or not np.all(np.isfinite(points)) or np.any(np.diff(points[:, 0]) <= 0):
        raise ValueError(f"Invalid or unsorted data: {path}")
    if relative and np.any((points[:, 1] < 0) | (points[:, 1] > 1)):
        raise ValueError(f"Expected probability/relative ordinate in [0,1]: {path}")
    return {"path": path, "x": points[:, 0], "y": points[:, 1], "sha256": sha256(path)}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canvas(title, subtitle, ylabel, log_length=False):
    fig = plt.figure(figsize=(SIZE[0] / DPI, SIZE[1] / DPI), dpi=DPI, facecolor="white")
    ax = fig.add_axes((0.115, 0.255, 0.845, 0.545))
    ax.set_xlim(*XLIM)
    ax.set_xticks(np.arange(XLIM[0], XLIM[1] + 1, 50))
    ax.xaxis.set_minor_locator(MultipleLocator(25))
    ax.set_xlabel("Lunghezza d’onda, λ [nm]", labelpad=10)
    ax.set_ylabel(ylabel, labelpad=10)
    if log_length:
        ax.set_yscale("log")
        ax.set_ylim(0.1, 1000)
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=5))
        ax.yaxis.set_minor_locator(LogLocator(base=10, subs=(2, 5), numticks=20))
        ax.yaxis.set_minor_formatter(NullFormatter())
    else:
        ax.set_ylim(0, 1)
        ax.set_yticks(np.linspace(0, 1, 6))
        ax.yaxis.set_minor_locator(MultipleLocator(0.1))
    ax.set_axisbelow(True)
    ax.grid(which="major", color="#DEE4EC", linewidth=0.8)
    ax.grid(which="minor", color="#EDF0F5", linewidth=0.5)
    ax.tick_params(which="major", direction="out", length=4)
    ax.tick_params(which="minor", direction="out", length=2)
    fig.text(0.115, 0.943, title, fontsize=19, weight="bold", va="top")
    fig.text(0.115, 0.886, subtitle, fontsize=12, color="#536174", va="top")
    return fig, ax


def draw_source(ax, dataset, color, label, linestyle="-", points=True):
    ax.plot(dataset["x"], dataset["y"], color=color, label=label, linestyle=linestyle)
    if points:
        ax.scatter(dataset["x"], dataset["y"], s=9, color=color, zorder=3,
                   linewidths=0, clip_on=True)


def draw_tio2(ax, dataset, label=True):
    # Interpolate R in ENERGY exactly like the source/runtime, then convert E to λ.
    end = HC_EV_NM / dataset["x"][0]
    wave = np.linspace(XLIM[0], min(XLIM[1], end), 1600)
    refl = np.interp(HC_EV_NM / wave, dataset["x"], dataset["y"])
    ax.plot(wave, refl, color=COLORS["tio2"], label="TiO₂ FNAL · tabella grezza" if label else None)
    ax.scatter(HC_EV_NM / dataset["x"], dataset["y"], s=16, color=COLORS["tio2"], zorder=3, linewidths=0)
    # Runtime extension is separated visually from the source support.
    if end < XLIM[1]:
        ax.plot([end, XLIM[1]], [dataset["y"][0]] * 2, color=COLORS["tio2"],
                linestyle="--", linewidth=1.4, label="Estensione a R costante" if label else None)


def draw_yang(ax, dataset, color, label):
    draw_source(ax, dataset, color, label)
    end = dataset["x"][-1]
    ax.plot([end, XLIM[1]], [dataset["y"][-1]] * 2, color=color, linestyle="--", linewidth=1.4)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=REPO / "reference/plots",
                        help="Central PNG gallery (default: reference/plots)")
    parser.add_argument("--no-adjacent-copies", action="store_true",
                        help="Only write central gallery; do not update PNGs beside data")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    s = "optics/scintillators/"
    y = "optics/y11/"
    c = "optics/coatings/"
    datasets = {
        "fnal": load_csv(s + "fnal_extruded.csv", "wavelength_nm", "relative_value"),
        "ej200": load_csv(s + "ej200.csv", "wavelength_nm", "relative_value"),
        "transmission": load_csv(s + "fnal_relative_transmission.csv", "wavelength_nm", "relative_value"),
        "y11_emission": load_csv(y + "y11_emission.csv", "wavelength_nm", "relative_value"),
        "y11_absorption": load_csv(y + "y11_absorption.csv", "wavelength_nm", "relative_value"),
        "tio2": load_csv(c + "fnal_tio2_energy.csv", "photon_energy_ev", "reflectance"),
        "tyvek": load_csv(c + "tyvek.csv", "wavelength_nm", "reflectance"),
        "mylar": load_csv(c + "aluminized_mylar.csv", "wavelength_nm", "reflectance"),
        "bc621": load_csv(c + "bc621_reference_points.csv", "wavelength_nm", "reflectivity_fraction"),
        "pde": load_csv("s13360_3075cs_pde.csv", "wavelength_nm", "pde_probability"),
    }
    manifest = {
        "description": "Plots of existing reference data, not new physics inputs or digitizations",
        "script": str(Path(__file__).relative_to(REPO)),
        "script_sha256": sha256(Path(__file__)),
        "versions": {"matplotlib": matplotlib.__version__, "numpy": np.__version__},
        "common_style": {"width_px": SIZE[0], "height_px": SIZE[1], "dpi": DPI,
                         "axes_rect": [0.115, 0.255, 0.845, 0.545],
                         "x_label": "Lunghezza d’onda, λ [nm]", "x_limits_nm": list(XLIM),
                         "x_scale": "linear", "default_y_limits": [0, 1], "default_y_scale": "linear",
                         "length_y_limits_mm": [0.1, 1000], "length_y_scale": "log"},
        "inputs": {key: {"path": str(d["path"].relative_to(REPO)), "sha256": d["sha256"]}
                   for key, d in datasets.items()},
        "plots": [],
    }

    def save(fig, ax, name, title, source_keys, source_note, notes, adjacent=None, log_length=False, english=False, legend_loc="lower right"):
        if ax.get_legend_handles_labels()[0]:
            ax.legend(loc=legend_loc, frameon=True, framealpha=0.96,
                      facecolor="white", edgecolor="#E1E7EF")
        fig.text(0.115, 0.151, notes, fontsize=10, va="top", linespacing=1.5)
        fig.text(0.115, 0.071, source_note, fontsize=9.5, color="#536174", va="top")
        fig.text(0.115, 0.032, ("MINE reference | Source data and declared approximations | No peak alignment" if english else "MINE · reference | Dati originali / derivazioni dichiarate · Nessun adattamento dei picchi"),
                 fontsize=8.5, color="#6A7686", va="bottom")
        # Fixed canvas: no bbox_inches='tight', which would vary dimensions by labels.
        fig.canvas.draw()
        renderer = fig.canvas.get_renderer()
        for text in fig.texts + [ax.xaxis.label, ax.yaxis.label]:
            bounds = text.get_window_extent(renderer)
            if bounds.x0 < 0 or bounds.y0 < 0 or bounds.x1 > SIZE[0] or bounds.y1 > SIZE[1]:
                raise ValueError(f"Text outside canvas in {name}: {text.get_text()}")
        path = output / name
        fig.savefig(path, dpi=DPI, metadata={"Title": title, "Description": notes, "Source": source_note})
        plt.close(fig)
        with Image.open(path) as image:
            if image.size != SIZE or image.format != "PNG":
                raise ValueError(f"Unexpected PNG format: {path}")
        copy = None
        if adjacent is not None and not args.no_adjacent_copies:
            copy = DATA / adjacent
            shutil.copyfile(path, copy)
        manifest["plots"].append({
            "file": name, "title": title, "inputs": source_keys,
            "y_label": ax.get_ylabel(), "y_scale": "log" if log_length else "linear",
            "y_limits": list(ax.get_ylim()), "source_note": source_note, "notes": notes,
            "sha256": sha256(path),
            "adjacent_copy": str(copy.relative_to(REPO)) if copy else None,
        })

    emission_ylabel = "Intensità di emissione relativa [u.a.]"
    for key, title, subtitle, source, note, name in [
        ("fnal", "Spettro di emissione — scintillatore estruso FNAL",
         "Polistirene + 1% PPO + 0.03% POPOP · fluorescenza con eccitazione UV a 310 nm",
         "S01 · Pla-Dalmau, Bross, Rykalin (2003), Fig. 4 · fnal_extruded.csv",
         "Massimo relativo = 1. Proxy di fluorescenza, non spettro MIP calibrato.\nLinee fra i dati digitalizzati; nessuna curva aggiunta fuori supporto.", "fnal_emission.png"),
        ("ej200", "Spettro di emissione — scintillatore colato EJ-200",
         "Eljen Technology · spettro relativo dal datasheet EJ-200",
         "S02 · Eljen, datasheet EJ-200 · ej200.csv",
         "Massimo relativo = 1. Il grafico della fonte termina a 500 nm con intensità non nulla.\nLa coda non pubblicata non è stata estrapolata.", "ej200_emission.png"),
    ]:
        fig, ax = canvas(title, subtitle, emission_ylabel)
        draw_source(ax, datasets[key], COLORS[key], "Dati digitalizzati")
        save(fig, ax, name, title, [key], source, note, s + name)

    title = "Trasmittanza relativa — scintillatore estruso FNAL"
    fig, ax = canvas(title, "Disco spesso 10 mm · riferimento: disco di polistirene puro di uguale spessore",
                     "Trasmittanza relativa T / T(PS) [1]")
    draw_source(ax, datasets["transmission"], COLORS["transmission"], "Trasmittanza rispetto al PS")
    save(fig, ax, "fnal_relative_transmission.png", title, ["transmission"],
         "S01 · Pla-Dalmau et al. (2003), Fig. 4 · fnal_relative_transmission.csv",
         "Dato di ricerca: non installato come assorbimento assoluto Geant4.\nIl riferimento PS impedisce di ricavare L assoluta senza conoscerne l’estinzione.",
         s + "fnal_relative_transmission.png")

    title = "Spettri e trasmittanza — scintillatori FNAL ed EJ-200"
    fig, ax = canvas(title, "Emissioni relative e trasmittanza FNAL: grandezze distinte, sovrapposte per confronto",
                     "Intensità relativa / trasmittanza [1]")
    draw_source(ax, datasets["fnal"], COLORS["fnal"], "FNAL · emissione", points=False)
    draw_source(ax, datasets["ej200"], COLORS["ej200"], "EJ-200 · emissione", points=False)
    draw_source(ax, datasets["transmission"], COLORS["transmission"], "FNAL · T / T(PS)", "--", False)
    save(fig, ax, "spectra_and_transmission.png", title, ["fnal", "ej200", "transmission"],
         "S01 · Pla-Dalmau et al. (2003), Fig. 4 | S02 · Eljen, datasheet EJ-200",
         "Emissioni al massimo relativo = 1; T / T(PS) conserva i valori pubblicati.\nNessuna resa assoluta confrontabile dall’altezza degli spettri.", s + "spectra_and_transmission.png")

    title = "Assorbimento relativo — fibra WLS Kuraray Y-11"
    fig, ax = canvas(title, "Coefficiente del colorante normalizzato a 430 nm · non lunghezza di attenuazione assiale",
                     "Coefficiente relativo A(λ) / A(430 nm) [1]")
    draw_source(ax, datasets["y11_absorption"], COLORS["absorption"], "Assorbimento del colorante")
    save(fig, ax, "y11_absorption.png", title, ["y11_absorption"],
         "S03 · Herzkamp (2016), Fig. 4.14 · y11_absorption.csv",
         "A(430 nm) = 1. Dati disponibili circa 350–490 nm.\nIl comportamento fuori supporto non è misurato dalla curva e non è disegnato.", y + "y11_absorption.png")

    title = "Spettro di emissione — fibra WLS Kuraray Y-11"
    fig, ax = canvas(title, "Emissione relativa del colorante · convenzione del modello: intensità di fotoni per nm",
                     emission_ylabel)
    draw_source(ax, datasets["y11_emission"], COLORS["y11"], "Dati digitalizzati")
    save(fig, ax, "y11_emission.png", title, ["y11_emission"],
         "S03 · Herzkamp (2016), Fig. 4.14 · y11_emission.csv",
         "Massimo relativo = 1. L’ordinata della fonte non distingue fotoni da potenza radiante.\nSupporto circa 430–640 nm; nessuna coda aggiunta.", y + "y11_emission.png")

    absorption = datasets["y11_absorption"]
    wave = np.unique(np.r_[np.linspace(absorption["x"][0], absorption["x"][-1], 4000), absorption["x"]])
    coefficient = 0.0147 * 200 * np.interp(wave, absorption["x"], absorption["y"])
    # Mask exact zero: do not draw the 1e15-mm numerical sentinel as a measurement.
    length = np.divide(1, coefficient, out=np.full_like(coefficient, np.nan), where=coefficient > 0)
    title = "Lunghezza di assorbimento WLS — Y-11 a 200 ppm"
    fig, ax = canvas(title, "Derivata dal coefficiente del colorante · cammino ottico reale, non distanza assiale",
                     "Lunghezza di assorbimento WLS [mm]", log_length=True)
    ax.plot(wave, length, color=COLORS["absorption"], label="L(λ) = 1 / [0.0147 × 200 × A(λ)]")
    positive = absorption["y"] > 0
    ax.scatter(absorption["x"][positive], 1 / (.0147 * 200 * absorption["y"][positive]),
               s=15, color=COLORS["absorption"], linewidths=0, zorder=3)
    save(fig, ax, "y11_wls_absorption_length_200ppm.png", title, ["y11_absorption"],
         "S03 · Herzkamp (2016), Fig. 4.14 e Tab. 4.4 | k₄₃₀ = 0.0147 ppm⁻¹ mm⁻¹",
         "Scala verticale logaritmica: 0.1–1000 mm. L(430 nm) = 0.340 mm.\nA = 0 omesso; nessuna coda estrapolata. Incertezza della scala assoluta: vedi reference/sources.md.",
         y + "y11_wls_absorption_length_200ppm.png", log_length=True)

    title = "Riflettanza — TiO₂ coestruso FNAL / NICADD"
    fig, ax = canvas(title, "Tabella grezza Mu2e attribuita ad Anna · rivestimento PS/TiO₂ dei contatori CRV",
                     "Riflettanza R [1]")
    draw_tio2(ax, datasets["tio2"])
    save(fig, ax, "fnal_tio2_reflectance.png", title, ["tio2"],
         "S08 · Mu2e CRVStandalone, commit c56260a | S09 · Ehrlich (2019)",
         "Punti della fonte convertiti da eV a nm; interpolazione lineare in energia.\nTratteggio: estensione di modello oltre 620 nm. Nessuna correzione empirica Mu2e applicata.",
         c + "fnal_tio2_reflectance.png")

    for key, title, subtitle, name in [
        ("tyvek", "Riflettanza relativa — Tyvek 150 µm",
         "Campione non irradiato · misura con sfera integratrice, incidenza di 10°", "tyvek_reflectance.png"),
        ("mylar", "Riflettanza relativa — Mylar alluminato 10 µm",
         "Campione non irradiato · misura con sfera integratrice, incidenza di 10°", "aluminized_mylar_reflectance.png"),
    ]:
        fig, ax = canvas(title, subtitle, "Riflettanza relativa R / R(BaSO₄) [1]")
        draw_yang(ax, datasets[key], COLORS[key], "Dati digitalizzati · riferimento BaSO₄")
        # Dashed line semantics are also encoded in the note, including overlays.
        save(fig, ax, name, title, [key], "S10 · Yang et al. (2021), Fig. 3(a) · dati relativi a BaSO₄",
             "Non è una calibrazione assoluta del contatto ottico MINE.\nTratteggio oltre 800 nm: estensione costante del modello; non nuovi dati della fonte.", c + name)

    title = "Riflettanza di riferimento — vernice BC-621"
    fig, ax = canvas(title, "Profilo storico · punti del datasheet e approssimazione scalare conservata",
                     "Riflettanza R [1]")
    bc = datasets["bc621"]
    ax.scatter(bc["x"], bc["y"], s=28, color=COLORS["bc621"], linewidths=0, zorder=3,
               label="Punti digitalizzati · solo riferimento")
    ax.axhline(.92, color=COLORS["bc621"], linestyle="--", linewidth=1.7,
               label="Modello storico · R = 0.92 costante")
    save(fig, ax, "bc621_reflectance.png", title, ["bc621"],
         "S13 · Saint-Gobain / Luxium, datasheet BC-621 (2021)",
         "I punti non costituiscono una curva spettrale attiva nel simulatore.\nVernice distinta dal TiO₂ coestruso FNAL; il valore scalare non descrive il taglio UV.", c + "bc621_reflectance.png")

    title = "Riflettanza spettrale — rivestimenti FNAL, Tyvek e Mylar"
    fig, ax = canvas(title, "Confronto degli input correnti · curve Yang relative a BaSO₄, riferimento ideale = 1",
                     "Riflettanza / riflettanza relativa [1]")
    draw_tio2(ax, datasets["tio2"], label=False)
    # Give each material one legend entry and keep endpoint extensions dashed.
    ax.lines[0].set_label("TiO₂ FNAL · tabella grezza")
    draw_yang(ax, datasets["tyvek"], COLORS["tyvek"], "Tyvek · R / R(BaSO₄)")
    draw_yang(ax, datasets["mylar"], COLORS["mylar"], "Al-Mylar · R / R(BaSO₄)")
    save(fig, ax, "coating_spectra.png", title, ["tio2", "tyvek", "mylar"],
         "S08–S09 · Mu2e / Ehrlich | S10 · Yang et al. (2021), Fig. 3(a)",
         "Confronto di input eterogenei, non misura assoluta sullo stesso pannello.\nTratteggi: estremi costanti nel modello. Nessun riscalamento per allineare i massimi.", c + "coating_spectra.png")

    title = "PDE — SiPM Hamamatsu S13360-3075CS"
    fig, ax = canvas(title, "Passo 75 µm · temperatura 25 °C · sovratensione nominale +3 V",
                     "Photon detection efficiency, PDE [1]")
    draw_source(ax, datasets["pde"], COLORS["pde"], "PDE tipica digitalizzata")
    save(fig, ax, "s13360_3075cs_pde.png", title, ["pde"],
         "S07 · Hamamatsu S13360 series, p. 4, KAPDB0325EB · s13360_3075cs_pde.csv",
         "270 nm: ancora nulla imposta; la traccia leggibile inizia circa a 280 nm.\nPDE include il fill factor; il ramo Geant4 corrente forza il SiPM ideale (questa curva non attiva).",
         "s13360_3075cs_pde.png")

    title = "Spettri di emissione — FNAL, EJ-200 e Kuraray Y-11"
    fig, ax = canvas(title, "Scintillatore estruso, scintillatore colato e fibra WLS · confronto delle forme spettrali",
                     emission_ylabel)
    for key, color, label in [("fnal", "fnal", "FNAL · fluorescenza UV"),
                              ("ej200", "ej200", "EJ-200 · emissione"),
                              ("y11_emission", "y11", "Y-11 · emissione WLS")]:
        draw_source(ax, datasets[key], COLORS[color], label, points=False)
    save(fig, ax, "emission_spectra_comparison.png", title, ["fnal", "ej200", "y11_emission"],
         "S01 · Pla-Dalmau et al. | S02 · Eljen | S03 · Herzkamp, Fig. 4.14",
         "Massimi relativi = 1 già nei CSV; le ampiezze non confrontano le rese assolute.\nNessuna estrapolazione, traslazione o nuova normalizzazione dei dati.")

    # Refractive indices have different units/range from probabilities. All
    # four index plots use exactly the same n axis, canvas and wavelength axis.
    groove_models={key:load_csv("optics/grooves/"+key+"_refractive_index.csv",
                               "wavelength_nm","refractive_index",relative=False)
                   for key in ("air","gel","glue")}
    for key,d in groove_models.items():
        manifest["inputs"]["groove_"+key]={"path":str(d["path"].relative_to(REPO)),"sha256":d["sha256"]}
    def index_canvas(title,subtitle):
        fig,ax=canvas(title,subtitle,"Indice di rifrazione di fase, n [1]")
        ax.set_ylim(1,1.65);ax.set_yticks(np.arange(1,1.651,.1));ax.yaxis.set_minor_locator(MultipleLocator(.05))
        return fig,ax
    def index_curve(ax,key):
        d=groove_models[key];color={"air":"#536174","gel":"#207849","glue":"#C55C12"}[key]
        label={"air":"Aria · indice costante","gel":"Gel G608N3 · fit di Cauchy","glue":"Colla EJ-500 · indice costante"}[key]
        if key=="gel":
            support=d["x"]>=402
            ax.plot(d["x"][support],d["y"][support],color=color,label=label)
            ax.plot(d["x"][~support],d["y"][~support],color=color,linestyle="--")
            ax.scatter([402,589.3],[1.4995,1.4646],s=24,color=color,zorder=3,linewidths=0)
        else:
            ax.plot(d["x"],d["y"],color=color,linestyle="--",label=label)
    for key,material,source,notes in [
        ("air","aria ambiente","S26 · NIST · indice tipico dell’aria",
         "n = 1.000277 costante: approssimazione; dipendenze da λ, temperatura e pressione omesse."),
        ("gel","gel ottico Thorlabs G608N3","S24 · Thorlabs G608N3 · specifiche del produttore",
         "n(λ) = 1.4338 + 10520 / λ² (λ in nm). Punti: valori del produttore.\nTratteggio sotto 402 nm: estrapolazione UV non validata del fit."),
        ("glue","colla ottica Eljen EJ-500","S25 · Eljen EJ-500 · datasheet luglio 2021",
         "n = 1.57 nominale: estensione costante del modello; dispersione non fornita dal produttore."),
    ]:
        title="Indice di rifrazione — "+material
        fig,ax=index_canvas(title,"Riempimento dei groove · assorbimento trascurato su input dell’utente")
        index_curve(ax,key)
        save(fig,ax,key+"_refractive_index.png",title,["groove_"+key],source,notes,
             "optics/grooves/"+key+"_refractive_index.png")
    title="Indice di rifrazione — aria, gel e colla ottica"
    fig,ax=index_canvas(title,"Materiali di riempimento dei groove · confronto degli indici di fase")
    for key in ("air","gel","glue"):index_curve(ax,key)
    save(fig,ax,"groove_refractive_indices.png",title,["groove_"+key for key in groove_models],
         "S24 · Thorlabs G608N3 | S25 · Eljen EJ-500 | S26 · NIST",
         "Tratteggi: indici costanti assunti per aria/colla; estrapolazione del gel sotto 402 nm.\nQueste curve non sono probabilità né lunghezze di attenuazione.","optics/grooves/groove_refractive_indices.png")

    # New manufacturer WLS spectra, same canvas and wavelength axes as the gallery.
    for kind in ("absorption","emission"):
        keys=["y11_"+kind]
        for short in ("ys6","ys4"):
            key=short+"_"+kind
            datasets[key]=load_csv("optics/kuraray_wls/"+key+".csv","wavelength_nm","relative_value")
            d=datasets[key];manifest["inputs"][key]={"path":str(d["path"].relative_to(REPO)),"sha256":d["sha256"]}
            keys.append(key)
        title="WLS "+kind+" spectra: Kuraray Y-11, YS-6 and YS-4"
        fig,ax=canvas(title,"Y-11: Herzkamp 2016 | YS: Kuraray meeting, 13 November 2026",
                     "Relative "+kind+" [normalized]")
        ax.set_xlabel("Wavelength [nm]")
        for key,color,label in zip(keys,("black","#2672B8","#D37821"),("Y-11","YS-6","YS-4")):
            draw_source(ax,datasets[key],color,label,points=False)
        save(fig,ax,"kuraray_wls_"+kind+".png",title,keys,
             "Kuraray p.7: solution spectra | Herzkamp Fig.4.14: Y-11",
             "YS curves: raster digitization, 2 nm grid; weak unresolved tails are truncated.\nRelative spectra do not determine absolute absorption or quantum yield.",
             "optics/kuraray_wls/wls_"+kind+".png",english=True)
    title="Kuraray spectral loss: YS-4, YS-6 and Y-11 proxy"
    fig,ax=canvas(title,"Solid: input extinction | Dashed: identical ordinary loss in core and cladding","Transmission loss [dB/km]")
    ax.set_xlabel("Wavelength [nm]");ax.set_ylim(0,2000);ax.set_yticks(np.arange(0,2001,500));ax.yaxis.set_minor_locator(MultipleLocator(250))
    keys=[]
    for short,color,label in (("ys6","#2672B8","YS-6"),("ys4","#D37821","YS-4"),("y11","black","Y-11 proxy")):
        key=short+"_transmission";keys.append(key)
        file=short+"_transmission_proxy.csv" if short=="y11" else short+"_transmission.csv"
        datasets[key]=load_csv("optics/kuraray_wls/"+file,"wavelength_nm","loss_db_per_km",relative=False)
        d=datasets[key];manifest["inputs"][key]={"path":str(d["path"].relative_to(REPO)),"sha256":d["sha256"]}
        draw_source(ax,d,color,label+" input",points=False)
        wave=np.linspace(300,900,1201);host=np.interp(np.maximum(wave,520),d["x"],d["y"])
        ax.plot(wave,host,color=color,ls="--",label=label+" ordinary")
    save(fig,ax,"kuraray_wls_transmission.png",title,keys,"Kuraray meeting, 13 November 2026, page 7; Y-11 is a declared substitute",
         "Y-11 proxy: mean of YS-4/YS-6 coefficients. Ordinary loss uses the red curve, held below 520 nm.\nThe blue excess is assigned to WLS reabsorption; no scalar 4 m loss is added.",
         "optics/kuraray_wls/wls_transmission.png",english=True,legend_loc="upper right")
    comp_path=DATA/"optics/kuraray_wls/loss_components.csv"
    with comp_path.open() as stream:components=list(csv.DictReader(stream))
    manifest["inputs"]["fiber_loss_components"]={"path":str(comp_path.relative_to(REPO)),"sha256":sha256(comp_path)}
    for short,label in (("y11","Y-11 substitute"),("ys6","YS-6"),("ys4","YS-4")):
        title="Spectral loss budget: Kuraray "+label
        fig,ax=canvas(title,"One extinction budget: ordinary absorption + WLS reabsorption","Extinction coefficient [dB/km]")
        ax.set_xlabel("Wavelength [nm]");ax.set_ylim(0,2000);ax.set_yticks(np.arange(0,2001,500));ax.yaxis.set_minor_locator(MultipleLocator(250))
        d=datasets[short+"_transmission"];draw_source(ax,d,"black","Input extinction",points=False)
        rows=[r for r in components if r["profile"]=={"y11":"Y-11","ys6":"YS-6","ys4":"YS-4"}[short]]
        wave=np.array([float(r["wavelength_nm"]) for r in rows]);ordinary=np.array([float(r["ordinary_rate_per_mm"]) for r in rows])/(np.log(10)/1e7)
        dye=np.array([float(r["wls_rate_per_mm"]) for r in rows])/(np.log(10)/1e7)
        ax.plot(wave,ordinary,color="#2672B8",ls="--",label="Ordinary: core = cladding")
        resolved=(wave>=d["x"][0])&(wave<=d["x"][-1])
        ax.plot(wave[resolved],dye[resolved],color="#D37821",ls=":",label="WLS: core only")
        save(fig,ax,"kuraray_"+short+"_loss_budget.png",title,[short+"_transmission","fiber_loss_components"],
             "Kuraray p.7; decomposition is an explicit model assumption, not separately measured PS/PMMA data",
             "Within measured support at 200 effective ppm, the two rates sum to the input curve.\nBlue absorption <=480 nm is retained; a log-coefficient bridge joins the weak red tail.",
             "optics/kuraray_wls/"+short+"_loss_budget.png",english=True,legend_loc="upper right")

    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    index = ["# Grafici delle reference MINE", "",
             "PNG **1800 × 1200 pixel**, 180 dpi, identica area del grafico e asse λ **250–900 nm**.",
             "Asse verticale **0–1 lineare** per intensità relative, coefficienti relativi, riflettanza, trasmittanza e PDE.",
             "Gli indici di rifrazione hanno limiti verticali comuni **1–1.65**, lineari e adimensionali.",
             "La lunghezza di assorbimento ha unità diverse: **0.1–1000 mm, logaritmico**; stessa scala orizzontale.", "",
             "Tutti i CSV spettrali presenti sono rappresentati. Non sono inventate curve per i parametri scalari.",
             "I valori di riflettanza/trasmittanza/PDE non sono rinormalizzati; gli spettri sono già relativi nei CSV.",
             "Non viene convertita la trasmittanza FNAL in lunghezza di assorbimento assoluta.",
             "Linee fra i punti: interpolazione della fonte/modello. Tratteggi dei rivestimenti: estensione a estremi costanti.", "",
             "I PNG individuali e i confronti di famiglia sono copiati identici accanto ai CSV; questa è la galleria centrale.",
             "[manifest.json](manifest.json) registra input/hash, scale, copie e formula della derivazione Y-11.", "",
             "Riprodurre dalla radice del repository (numpy, matplotlib e Pillow):", "", "```sh",
             "python3 scripts/plot_mine_references.py", "```", "",
             "Per una prova in un'altra cartella senza aggiornare le copie presso i dati:", "", "```sh",
             "python3 scripts/plot_mine_references.py --output-dir /tmp/mine-reference-plots --no-adjacent-copies",
             "```", "", "Fonti e limiti: [catalogo](../sources.md), [decisioni](../decisions.md).", ""]
    for plot in manifest["plots"]:
        index += ["## " + plot["title"], "", f'![{plot["title"]}]({plot["file"]})', ""]
    (output / "README.md").write_text("\n".join(index))
    print(f"Created {len(manifest['plots'])} PNGs ({SIZE[0]}x{SIZE[1]}) in {output}")
    print(f"Input CSVs: {len(manifest['inputs'])}; adjacent copies: {sum(p['adjacent_copy'] is not None for p in manifest['plots'])}")


if __name__ == "__main__":
    main()
