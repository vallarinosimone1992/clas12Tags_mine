#!/usr/bin/env python3
"""Extract source-specific coating curves, with no peak shift or normalization.

FNAL TiO2: untuned table attributed to Anna in the Mu2e CRVStandalone code.
Tyvek and Al Mylar: Yang et al., NIMA 992 (2021) 165043, Fig.3(a).
Requires Poppler, Pillow, and Python stdlib. No runtime CSV dependencies.
"""
import argparse,base64,csv,hashlib,json,re,statistics,subprocess,tempfile
from pathlib import Path
from xml.etree import ElementTree as ET
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--yang-pdf',required=True,type=Path)
    p.add_argument('--mu2e-source',required=True,type=Path)
    p.add_argument('--output-dir',required=True,type=Path)
    args=p.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        svg=Path(directory)/'page.svg'
        subprocess.run(['pdftocairo','-f','2','-l','2','-svg',str(args.yang_pdf),str(svg)],check=True)
        images=[e for e in ET.parse(svg).getroot().iter() if e.tag.endswith('image') and e.get('width')=='1309' and e.get('height')=='1386']
        if len(images)!=1:raise ValueError('Expected Yang Fig.3 embedded raster')
        raw=base64.b64decode(images[0].get('{http://www.w3.org/1999/xlink}href').split(',',1)[1])
        imagepath=Path(directory)/'figure.png';imagepath.write_bytes(raw)
        graph=Image.open(imagepath).convert('RGB')
        curves={}
        for material in ('tyvek','aluminized_mylar'):
            data=[]
            for wavelength in range(250,801,5):
                x=154+(wavelength-200)*(1274-154)/600;ys=[]
                for col in range(round(x)-1,round(x)+2):
                    for y in range(160,391):
                        r,g,b=graph.getpixel((col,y))
                        selected=(r>180 and b>180 and g<130) if material=='tyvek' else (r>180 and g<100 and b<100)
                        if selected:ys.append(y)
                if not ys:
                    if material=='aluminized_mylar' and 385<=wavelength<=395:continue # ESR curve overlaps the red trace here.
                    raise ValueError(f'Missing {material} point at {wavelength} nm')
                value=(626-statistics.median(ys))/(626-170)
                if not 0<=value<=1:raise ValueError('Invalid relative reflectance')
                data.append((float(wavelength),value))
            curves[material]=data
    source=args.mu2e_source.read_text()
    def array(name):
        match=re.search(r'G4double\s+'+name+r'\[11\]\s*=\s*\{([^}]+)\}',source)
        if not match:raise ValueError(f'Missing {name} array')
        return [float(re.sub(r'\*eV','',part.strip())) for part in match[1].split(',')]
    energies,reflection=array('p_TiO2'),array('refl_TiO2')
    curves['fnal_tio2_energy']=list(zip(energies,reflection))
    args.output_dir.mkdir(parents=True,exist_ok=True)
    for name,data in curves.items():
        with (args.output_dir/(name+'.csv')).open('w') as stream:
            stream.write('# Source values; see spectral_provenance.json. No peak shifting or amplitude normalization.\n')
            writer=csv.writer(stream)
            writer.writerow(['photon_energy_ev' if name.endswith('_energy') else 'wavelength_nm','reflectance'])
            writer.writerows((f'{x:.8g}',f'{y:.12g}') for x,y in data)
    commit='c56260aa5151332939764c2ac3f7b2e69c10e78e'
    provenance={
      'version':'Mu2eAnnaRaw_Yang2021_Fig3a_v1',
      'fnal_tio2':{
        'url':f'https://github.com/Mu2e/CRVStandalone/blob/{commit}/wls/src/WLSDetectorConstruction.cc#L300',
        'commit':commit,'source_sha256':hashlib.sha256(args.mu2e_source.read_bytes()).hexdigest(),
        'companion_paper':'https://arxiv.org/abs/1910.04846',
        'identity':'Surface intended for FNAL/NICADD coextruded PS/TiO2 CRV counters; baseline numbers attributed to Anna in the code',
        'method':'Exact untuned p_TiO2 and refl_TiO2 arrays; original linear-energy interpolation',
        'not_imported':'Mu2e subsequently applies R_tuned=1-0.75*(1-R_raw) to match its test beam; that empirical correction is NOT applied here',
        'limits':['No original instrument/batch calibration accompanies these code values',
                  'High-energy plateau above 3.44 eV is an explicit source assumption',
                  'Angular reflection model is not copied from Mu2e; MINE uses its documented Lambertian approximation',
                  'Companion paper describes 0.25 mm, 30 wt% TiO2 coating; not identical to every FNAL/MINOS production']},
      'yang2021':{
        'url':'https://www.its.caltech.edu/~rzhu/papers/21_nim_Reflectance.pdf',
        'doi':'10.1016/j.nima.2021.165043','source_pdf_sha256':hashlib.sha256(args.yang_pdf.read_bytes()).hexdigest(),
        'figure':'3(a), PDF page 2, unirradiated Tyvek 150 um / aluminized Mylar 10 um',
        'measurement':'Integrating sphere, 10 deg incidence, relative to BaSO4 reference',
        'systematic_uncertainty':'about 0.01 absolute in relative reflectance, 250-800 nm',
        'method':'Embedded 1309x1386 raster; colored curve pixel medians, 5 nm sampling, no smoothing; omit red trace where occluded near 390 nm',
        'calibration_pixels':{'x200':154,'x800':1274,'y0':626,'y100':170},
        'pixel_selection':{'columns':'round(x) +/- 1','y_range':[160,390],'tyvek':'R>180,B>180,G<130','mylar':'R>180,G<100,B<100'},
        'limits':['Curves are relative reflectance, not an absolute measurement of MINE per-contact survival',
                  'Runtime multiplies by coatingReferenceAlbedo; unity is an explicit ideal-reference approximation/upper envelope',
                  'Do not generalize these particular film samples to every Mylar/Tyvek grade',
                  'Tyvek is modeled as Lambertian only as an effective approximation; measured BRDF can differ']},
      'runtime':'Probability ordinate: NO spectral-density Jacobian. Clamp unresolved endpoints outside support; generated FNAL/EJ200/Y11 spectra are inside measured Yang support. No translations or peak matching.'}
    (args.output_dir/'spectral_provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    header='// Generated by scripts/extract_mine_coating_spectra.py; see coatings/spectral_provenance.json.\n// Mu2e table attribution: Mu2e CRVStandalone (Apache-2.0), untuned values only.\n#pragma once\n#include <array>\n#include <utility>\nnamespace mine { namespace coatingdata {\ninline constexpr const char* version="Mu2eAnnaRaw_Yang2021_Fig3a_v1";\n'
    for name,data in curves.items():
        header+=f'inline constexpr std::array<std::pair<double,double>,{len(data)}> {name} {{{{\n'
        header+=''.join(f'    {{{x:.10g}, {y:.12g}}},\n' for x,y in data)+'}};\n'
    header+='}} // namespace mine::coatingdata\n'
    (args.output_dir/'mine_coating_spectral_data.h').write_text(header)
    print({name:len(data) for name,data in curves.items()})

if __name__=='__main__':main()
