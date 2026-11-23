#!/usr/bin/env python3
"""Extract FNAL/NICADD Fig.4 and Eljen EJ-200 emission vector curves.

Requires Poppler pdftocairo; no fitted or extrapolated spectral tails.
"""
import argparse
import csv
import hashlib
import json
import re
import statistics
import subprocess
import tempfile
from pathlib import Path
from xml.etree import ElementTree as ET


def paths(pdf, page):
    with tempfile.TemporaryDirectory() as directory:
        svg = Path(directory) / 'page.svg'
        subprocess.run(['pdftocairo', '-f', str(page), '-l', str(page), '-svg',
                        str(pdf), str(svg)], check=True)
        return [p for p in ET.parse(svg).getroot().iter() if p.tag.endswith('path')]


def vertices(path):
    d = path.get('d', '')
    if re.search('[CcQqAaHhVv]', d):
        raise ValueError('Expected polygonal vector curve')
    values = list(map(float, re.findall(r'[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?', d)))
    return list(zip(values[::2], values[1::2]))


def fnal_curve(elements, color):
    points = set()
    for path in elements:
        # Two-vertex paths are the data segments; exclude overlaid markers.
        if path.get('stroke') == color and re.fullmatch(
                r'M [-\d.]+ [-\d.]+ L [-\d.]+ [-\d.]+ ', path.get('d', '')):
            points.update(vertices(path))
    grouped = {}
    for x, y in points:
        if 354.75 <= x <= 524.25:
            grouped.setdefault(x, []).append(y)
    if len(grouped) < 80:
        raise ValueError('FNAL Fig.4 vector curve not found')
    # Published vectors are quantized in 0.75 pt. Collapse repeated wavelengths
    # to the median of their distinct ordinates, without smoothing in lambda.
    return [(300 + (x - 354.75) * 250 / (524.25 - 354.75),
             max(0., statistics.median(ys) - 648)) for x, ys in sorted(grouped.items())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fnal-pdf', required=True, type=Path)
    parser.add_argument('--ej200-pdf', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    args = parser.parse_args()
    for pdf in (args.fnal_pdf, args.ej200_pdf):
        if pdf.read_bytes()[:5] != b'%PDF-':
            parser.error(f'Not a PDF: {pdf}')
    elements = paths(args.fnal_pdf, 3)
    fnal = fnal_curve(elements, 'rgb(0%, 0%, 50.19989%)')
    peak = max(y for x, y in fnal)
    fnal = [(x, y / peak) for x, y in fnal]
    # Extend only along the zero red baseline already shown in the plot.
    if fnal[-1][0] != 550:
        fnal.append((550., 0.))
    transmission = [(x, y / 66.) for x, y in
                    fnal_curve(elements, 'rgb(100%, 0%, 100%)')]
    points = set()
    for p in paths(args.ej200_pdf, 1):
        if p.get('stroke-width') == '1.922' and 'Z' not in p.get('d', ''):
            v = vertices(p)
            if all(200 < x <= 415.51 and 159.47 <= y <= 300.13 for x, y in v):
                points.update(v)
    if len(points) < 30:
        raise ValueError('EJ-200 vector curve not found')
    ej200 = [(380., 0.)] + [
        (380 + (x - 173.460938) * 120 / (415.5 - 173.460938),
         max(0., (y - 159.480469) / (300.121094 - 159.480469)))
        for x, y in sorted(points)]
    spectra = {'fnal_extruded': fnal, 'ej200': ej200}
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in {**spectra, 'fnal_relative_transmission': transmission}.items():
        if any(b[0] <= a[0] for a, b in zip(data, data[1:])):
            raise ValueError(f'Non-increasing wavelengths: {name}')
        with (args.output_dir / f'{name}.csv').open('w') as stream:
            stream.write('# Digitized published vector curve; see provenance.json and README.md\n')
            writer = csv.writer(stream)
            writer.writerow(['wavelength_nm', 'relative_value'])
            writer.writerows((f'{x:.6f}', f'{y:.12g}') for x, y in data)
    metadata = {
        'version': 'FNAL2003_EljenEJ200_v1',
        'sources': {
            'fnal_extruded': {
                'url': 'https://lss.fnal.gov/archive/2003/conf/fermilab-conf-03-318-e.pdf',
                'title': 'Pla-Dalmau, Bross, Rykalin, Extruding Plastic Scintillator at Fermilab (2003)',
                'figure': '4, PDF page 3',
                'pdf_sha256': hashlib.sha256(args.fnal_pdf.read_bytes()).hexdigest(),
                'composition': 'Polystyrene + 1% PPO + 0.03% POPOP',
                'measurement': 'Front-surface fluorescence, 310 nm excitation, 1 cm disk',
                'calibration': {'x300': 354.75, 'x550': 524.25, 'y0': 648, 'yT100': 714},
                'limitations': [
                    'Fluorescence proxy, not a measured charged-particle scintillation spectrum',
                    'PPO and POPOP bands are both retained; no arbitrary UV cut',
                    'UV transfer/re-emission inside the scintillator is not modeled',
                    'Y11 absorption below its measured support (about 350 nm) remains unresolved/zero',
                    'Transmission is relative to an equal-thickness polystyrene reference, not absolute',
                    'Vector quantization ~1 nm; median ordinate at repeated wavelengths']},
            'ej200': {
                'url': 'https://courses.washington.edu/phys433/equipment/EJ200data-sheet.pdf',
                'official_current_datasheet': 'https://eljentechnology.com/images/products/data_sheets/EJ-200_EJ-204_EJ-208_EJ-212.pdf',
                'title': 'Eljen Technology, EJ-200 Plastic Scintillator, emission spectrum',
                'pdf_sha256': hashlib.sha256(args.ej200_pdf.read_bytes()).hexdigest(),
                'calibration': {'x380': 173.460938, 'x500': 415.5, 'y0': 159.480469, 'y1': 300.121094},
                'limitations': ['Curve stops at 500 nm with a nonzero ordinate (~0.055); unmeasured red tail is excluded, not extrapolated',
                                'Selecting this spectrum does not change yield, decay, Birks, index or material composition']}
        },
        'emission_convention': 'Relative photon density per nm assumed; power_per_nm can be selected explicitly',
        'runtime': 'Linear interpolation in wavelength; Jacobian to photon density per energy; 0.25 nm grid with source vertices and guard points; zero outside source support',
        'attenuation': {
            'implemented': 'Existing wavelength-independent bulkAbsorptionLengthMm retained',
            'fnal': 'Trelative measures excess extinction: mu_sample - mu_PS = -ln(Trelative)/10 mm; PS baseline is unknown',
            'ej200': 'Eljen 380 cm is a technical light attenuation length, not an absolute spectral ABSLENGTH',
            'other_reference': 'https://pmc.ncbi.nlm.nih.gov/articles/PMC10746763/',
            'other_reference_limit': 'Fig.8 absorbance curves normalized to unity; absolute scale unavailable'}
    }
    (args.output_dir / 'provenance.json').write_text(json.dumps(metadata, indent=2) + '\n')
    header = '// Generated by scripts/extract_mine_scintillator_spectra.py; see optics/scintillators/provenance.json.\n#pragma once\n#include <array>\n#include <utility>\nnamespace mine { namespace scintdata {\ninline constexpr const char* version="FNAL2003_EljenEJ200_v1";\n'
    for name, data in spectra.items():
        header += f'inline constexpr std::array<std::pair<double,double>,{len(data)}> {name} {{{{\n'
        header += ''.join(f'    {{{x:.6f}, {y:.12g}}},\n' for x, y in data)
        header += '}};\n'
    header += '}} // namespace mine::scintdata\n'
    (args.output_dir / 'mine_scintillation_spectral_data.h').write_text(header)
    print('Extracted nodes:', {name: len(data) for name, data in spectra.items()})


if __name__ == '__main__':
    main()
