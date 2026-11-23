#!/usr/bin/env python3
"""Compare native monochromatic transmission against digitized Kuraray curves.
The unshifted primary count tests extinction; WLS children remain tracked in the
separate full-lineage count. Broad-spectrum attenuation is diagnostic, not forced.
"""
import argparse,csv,hashlib,json,math
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from analyze_mine_fiber_attenuation import fit


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
 target=a.output_dir/'spectral_loss_summary.json'
 if target.exists():raise ValueError('Refusing to overwrite '+str(target))
 curves={}
 for name in ('ys4','ys6'):
  f=a.input_dir/f'source-{name}-transmission.csv';d=np.genfromtxt(f,delimiter=',',names=True);curves[name]=(d['wavelength_nm'],d['loss_db_per_km'])
 def loss(profile,lam):
  if profile=='y11':return .5*(loss('ys4',lam)+loss('ys6',lam))
  x,y=curves[profile];return float(np.interp(lam,x,y))
 def read(path):
  r=list(csv.DictReader(path.open()));z=np.array([float(x['distance_mm']) for x in r])/1000;n=np.array([int(x['unshifted_primary_crossings']) for x in r]);b=np.array([int(x['first_fiber_crossings']) for x in r]);N=int(r[0]['generated'])
  if np.any(np.diff(n)>0) or np.any(n>b) or np.any(np.diff(b)>0):raise ValueError('Invalid lineage accounting')
  return z,n,b,N
 report={'reference':'Kuraray meeting 13 November 2026 p.7; digitized transmission loss','y11_proxy':'Arithmetic mean of YS4 and YS6 loss coefficients [dB/km], not lengths','host_anchor_nm':520,'blue_join_nm':480,'scalar_4m_loss':False,'time_cut':None,
         'spectral_observable':'First forward crossing by original unshifted primary; WLS descendants tracked in separate broadband count','reference_geometry':'Near-axis monochromatic launch; path equals axial distance; no input/output coupling factor',
         'acceptance':'All planes: |p_native-p_reference| < 6 binomial standard errors + 0.002 absolute; model-input consistency, not experimental validation',
         'tests':[],'broadband_diagnostic':{},'inputs':{}}
 for path in sorted(a.input_dir.glob('mono-*.csv')):
  _,profile,kind,w=path.stem.split('-');wave=float(w);z,n,b,N=read(path)
  db=loss(profile,max(520,wave) if kind=='cladding' else wave);length=4342.944819/db;pred=np.exp(-(z-.00001)/length)
  observed=n/N;error=np.sqrt(pred*(1-pred)/N);res=np.abs(observed-pred);allowed=6*error+.002
  f=fit(z,n)
  record={'profile':profile,'launch':kind,'wavelength_nm':wave,'generated':N,'reference_db_per_km':db,'reference_length_m':length,
          'native_length_m':f['length_m'],'native_length_standard_error_m':f['standard_error_m'],'max_abs_probability_residual':float(max(res)),
          'passed':bool(np.all(res<allowed)),'distance_m':z.tolist(),'unshifted_probability':observed.tolist(),'reference_probability':pred.tolist(),
          'all_lineage_probability':(b/N).tolist()}
  if kind=='cladding':record['passed']=record['passed'] and bool(np.array_equal(n,b))
  report['tests'].append(record);report['inputs'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
 fig,ax=plt.subplots(figsize=(9,6),dpi=200)
 for profile,label,color in [('y11','Y-11 proxy','black'),('ys6','YS-6','tab:blue'),('ys4','YS-4','tab:orange')]:
  lo=max(curves[k][0][0] for k in ('ys4','ys6')) if profile=='y11' else curves[profile][0][0]
  xx=np.linspace(lo,699.502,600);ax.plot(xx,[4342.944819/loss(profile,x) for x in xx],color=color,label=label+' reference')
  rows=[r for r in report['tests'] if r['profile']==profile and r['launch']=='core']
  ax.errorbar([r['wavelength_nm'] for r in rows],[r['native_length_m'] for r in rows],yerr=[r['native_length_standard_error_m'] for r in rows],fmt='o',ms=3,color=color,capsize=2)
 ax.set(xlim=(480,705),ylim=(.5,100),yscale='log',xlabel='Wavelength [nm]',ylabel='Monochromatic attenuation length [m]',title='Straight fiber: native spectral transmission vs Kuraray')
 ax.legend(fontsize=9);ax.grid(which='both',alpha=.25);fig.text(.5,.01,'Lines: source curves | Points: Geant4 unshifted transmission | Y-11: explicit substitute curve',ha='center',fontsize=8);fig.tight_layout(rect=(0,.025,1,1));fig.savefig(a.output_dir/'spectral_transmission_validation.png');plt.close(fig)
 fig,ax=plt.subplots(figsize=(9,6),dpi=200)
 for profile,label,color,nominal in [('y11','Y-11','black',3.8),('ys6','YS-6','tab:blue',3.8),('ys4','YS-4','tab:orange',3.4)]:
  path=a.input_dir/f'broadband-{profile}.csv';z,n,b,N=read(path);f=fit(z,b);f['nominal_zero_dose_graph_m']=nominal
  f['relative_to_zero_dose_graph']=f['length_m']/nominal;f['note']='Diagnostic effective fit in 1-6 m window; spectral mixture is not necessarily exponential; not a pass/fail calibration target'
  report['broadband_diagnostic'][label]=f;report['inputs'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
  ax.errorbar(z,b/N,np.sqrt((b/N)*(1-b/N)/N),fmt='o-',color=color,ms=3,capsize=2,label=f"{label}: effective L = {f['length_m']:.1f} m")
 ax.set(xlim=(0,6.2),ylim=(0,1),xlabel='Axial distance [m]',ylabel='Full-lineage first-crossing probability',title='Straight fiber: full emission spectrum and WLS reabsorption')
 ax.grid(alpha=.25);ax.legend();fig.text(.5,.01,'No forced 4 m attenuation | Forward guided-cone source | No time cut | Curves are spectral mixtures',ha='center',fontsize=8);fig.tight_layout(rect=(0,.025,1,1));fig.savefig(a.output_dir/'broadband_transmission_diagnostic.png');plt.close(fig)
 report['passed']=all(r['passed'] for r in report['tests']);target.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
 print('SPECTRAL_TRANSMISSION_PASS',report['passed'],'tests',len(report['tests']))
 for key,value in report['broadband_diagnostic'].items():print(key,'broadband effective L:',round(value['length_m'],3),'m; nominal graph:',value['nominal_zero_dose_graph_m'],'m')
 if not report['passed']:raise SystemExit('Native spectral validation FAILED')
if __name__=='__main__':main()
