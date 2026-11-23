#!/usr/bin/env python3
"""Generate versioned Kuraray transmission arrays from the digitized source CSVs.
No fitting or renormalization. Y11 uses the arithmetic mean of attenuation
coefficients (dB/km), restricted to the common YS4/YS6 measured domain.
"""
import argparse,bisect,csv,hashlib,json
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--data-dir',type=Path,default=Path(__file__).resolve().parents[1]/'experiments/mine/data/optics/kuraray_wls');p.add_argument('--header',type=Path);a=p.parse_args();d=a.data_dir
 header='// Generated from Kuraray 2026-11-13 p.7 transmission CSVs; do not edit.\n#pragma once\n#include <array>\n#include <utility>\nnamespace mine { namespace transmissiondata {\ninline constexpr const char* version="Kuraray20261113_transmission_budget_v1";\n'
 inputs={};curves={}
 for profile in ('ys4','ys6'):
  source=d/(profile+'_transmission.csv');rows=[(float(x['wavelength_nm']),float(x['loss_db_per_km'])) for x in csv.DictReader(source.open())]
  if not all(y>0 for x,y in rows) or not all(b[0]>a[0] for a,b in zip(rows,rows[1:])):raise ValueError('Invalid loss curve')
  header+=f'inline constexpr std::array<std::pair<double,double>,{len(rows)}> {profile} {{{{\n'+''.join(f'    {{{x:.3f},{y:.3f}}},\n' for x,y in rows)+'}};\n'
  curves[profile]=rows
  inputs[profile]={'file':source.name,'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'support_nm':[rows[0][0],rows[-1][0]]}
 lo=max(rows[0][0] for rows in curves.values());hi=min(rows[-1][0] for rows in curves.values())
 xs=sorted({x for rows in curves.values() for x,y in rows if lo<=x<=hi}|{lo,hi})
 def ordinate(rows,x):
  j=bisect.bisect_left([a for a,b in rows],x)
  if j==0:return rows[0][1]
  if j==len(rows):return rows[-1][1]
  a,b=rows[j-1];c,e=rows[j];return b+(e-b)*(x-a)/(c-a)
 proxy=d/'y11_transmission_proxy.csv'
 with proxy.open('w') as stream:
  writer=csv.writer(stream);writer.writerow(['wavelength_nm','loss_db_per_km'])
  writer.writerows((f'{x:.3f}',f'{.5*(ordinate(curves["ys4"],x)+ordinate(curves["ys6"],x)):.9f}') for x in xs)
 header+='}}\n';target=a.header or d/'mine_kuraray_transmission_data.h';target.write_text(header)
 metadata={'source':'Kuraray meeting 13 November 2026, page 7, transmission loss','inputs':inputs,'source_pdf_sha256':json.loads((d/'provenance.json').read_text())['source_pdf_sha256'],'generator':'scripts/generate_mine_kuraray_transmission.py','generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'units':'source dB/km; alpha[mm^-1] = A*ln(10)/1e7; L[mm]=1/alpha','y11_proxy':'Arithmetic mean of YS4 and YS6 in attenuation coefficients, on common measured support; NOT a Y11 measurement','assumptions':{'axial_mapping':'Near-axis monochromatic extinction: actual ray path approximately equals axial distance. Geant4 still tracks actual path for other angles.','ordinary_common':'For lambda>=520 nm use source transmission loss. Below 520 nm hold the 520-nm ordinate; identical ABSLENGTH for core and cladding.','wls_red_tail':'From first measured transmission wavelength to 520 nm allocate source loss minus ordinary loss to WLS; zero beyond 520 nm; no second copy of the same extinction.','blue_join':'Preserve original dye absorption through 480 nm, join logarithmically in coefficient to the transmission-derived tail at first measured wavelength.','concentration':'Tail calibrated at effective 200 ppm; scale WLS only with C/200 for systematic changes.','endpoints':'Ordinary loss held constant beyond last measured wavelength and below 520 nm; no claim of measured UV/IR behavior.','interpretation':'Transmission curve interpreted as removal from original wavelength/mode; re-emitted photons are tracked separately, not silently counted as unshifted transmission.'},'limits':['Core/cladding loss equality is user-authorized effective approximation, not equality of refractive indices or chemical composition','Decomposition into host and weak WLS tail is not uniquely measured; manufacturer spectral bandwidth/mode protocol unspecified','Native spectral check tests implementation against input curves, not experimental validation of the decomposition','No scalar 4-m correction is added, and no integral attenuation length is forced']}
 (d/'transmission_provenance.json').write_text(json.dumps(metadata,indent=2)+'\n');print('Generated',target)
if __name__=='__main__':main()
