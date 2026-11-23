import argparse,base64,csv,hashlib,json,subprocess,tempfile
from pathlib import Path
from xml.etree import ElementTree as ET
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--pdf',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as t:
 svg=Path(t)/'p.svg';subprocess.run(['pdftocairo','-f','7','-l','7','-svg',str(a.pdf),str(svg)],check=True)
 root=ET.parse(svg).getroot()
 imgs=[]
 for e in root.iter():
  if e.tag.endswith('image') and e.get('width')=='752' and e.get('height')=='452':
   raw=base64.b64decode(e.get('{http://www.w3.org/1999/xlink}href').split(',')[1]);q=Path(t)/(e.get('id')+'.png');q.write_bytes(raw)
   im=Image.open(q)
   if im.mode=='RGB': imgs.append((q,im.copy()))
 if len(imgs)!=2:raise ValueError('Expected two RGB plot images')
 spectra=np.asarray(imgs[0][1]);loss=np.asarray(imgs[1][1])
 (out/'source_spectra.png').write_bytes(imgs[0][0].read_bytes());(out/'source_transmission.png').write_bytes(imgs[1][0].read_bytes())
 result={}
 for profile,col in [('ys4',(244,177,131)),('ys6',(201,201,201))]:
  mask=np.all(spectra==col,axis=2)
  for kind,ymin,ymax,sign in [('absorption',219,411,1),('emission',24,215,-1)]:
   points=[]
   for x in range(107,714):
    ys=np.flatnonzero(mask[ymin:ymax+1,x])+ymin
    if x>540 and kind=='emission':ys=ys[ys>180] # remove legend
    if len(ys):points.append((300+(x-106)*400/608,sign*(217-float(np.median(ys)))/-193))
   # Absorption is plotted below zero. Its sign is reversed here.
   pts=np.array(points);pts[:,1]=np.clip(pts[:,1],0,1)
   if kind=='absorption':pts=pts[(pts[:,0]>=300)&(pts[:,0]<=500)]
   xs=np.arange(300 if kind=='absorption' else 400,502 if kind=='absorption' else 702,2.)
   if kind=='emission':
    # Below the two-pixel ordinate floor, declare zero rather than inventing
    # a long weak tail to 700 nm. This is a documented digitization limit.
    pts=np.vstack((pts,[pts[-1,0]+2,0]))
   # Baseline endpoints are explicit digitization assumptions; do not invent a red tail.
   pts=np.vstack(([xs[0],0],pts,[xs[-1],0]));pts=pts[np.argsort(pts[:,0],kind='stable')]
   ys=np.interp(xs,pts[:,0],pts[:,1]);ys/=max(ys)
   vals=list(zip(xs.tolist(),ys.tolist()));result[profile+'_'+kind]=vals
   with (out/(profile+'_'+kind+'.csv')).open('w') as f:
    f.write('wavelength_nm,relative_value\n');csv.writer(f).writerows((f'{x:.1f}',f'{y:.7f}') for x,y in vals)
   print(profile,kind,'traced range',points[0][0],points[-1][0],'peak',xs[np.argmax(ys)])
 # Keep original transmission data as a benchmark, not a second WLS absorber.
 for profile,col in [('ys4',(244,177,131)),('ys6',(201,201,201))]:
  m=np.all(loss==col,axis=2);pts=[]
  for x in range(113,714):
   ys=np.flatnonzero(m[25:383,x])+25
   if x>600:ys=ys[ys>160] # remove legend
   if len(ys):pts.append((400+(x-112)*300/602,(384-float(np.median(ys)))*2000/360))
  with (out/(profile+'_transmission.csv')).open('w') as f:
   f.write('wavelength_nm,loss_db_per_km\n');csv.writer(f).writerows((f'{x:.3f}',f'{y:.3f}') for x,y in pts)
 h='// Generated from Kuraray meeting 2026-11-13, page 7; raster digitization.\n#pragma once\n#include <array>\n#include <utility>\nnamespace mine { namespace kuraraydata {\ninline constexpr const char* version="Kuraray20261113_page7_raster_v1";\n'
 for name,pts in result.items():
  h+=f'inline constexpr std::array<std::pair<double,double>,{len(pts)}> {name} {{{{\n'+''.join(f'    {{{x:.1f}, {y:.7f}}},\n' for x,y in pts)+'}};\n'
 h+='}}\n';(out/'mine_kuraray_wls_data.h').write_text(h)
 prov={'source':'Kuraray PSF meeting 13 November 2026, page 7','source_pdf_sha256':hashlib.sha256(a.pdf.read_bytes()).hexdigest(),'method':'Poppler SVG embedded RGB raster; exact color pixel medians, linear interpolation through dashed/occluded gaps; sampled every 2 nm; no fitted peaks','colors':{'YS-4':[244,177,131],'YS-6':[201,201,201]},'axes_spectra':{'lambda300_x':106,'lambda700_x':714,'y_zero':217,'unit_height_pixels':193},'axes_transmission':{'lambda400_x':112,'lambda700_x':714,'y_zero':384,'y_2000':24},'limitations':['Approximate raster curves, about 0.66 nm/pixel and 0.0052 relative ordinate/pixel; dashes and occlusion add uncertainty','Spectra in styrene monomer solution, not measured bulk absorption of solid fiber','Relative absorption normalized at each maximum; absolute peak cross section for YS unknown','Shared peak absorption coefficient 0.0147 ppm^-1 mm^-1 and effective 200 ppm are USER-AUTHORIZED TRANSFER ASSUMPTIONS from Y11, not Kuraray YS measurements','Photon/nm emission convention assumed; alternative power/nm supported','Zero dye coefficient outside source support; no invented weak tail','Emission below the two-pixel ordinate floor is truncated to zero within 2 nm of the last resolved point; tails there are unmeasured','Transmission loss is aggregate fiber data and is NOT installed as ordinary ABSLENGTH on top of WLS']}
 (out/'provenance.json').write_text(json.dumps(prov,indent=2)+'\n')
