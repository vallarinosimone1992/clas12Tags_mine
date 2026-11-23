#!/usr/bin/env python3
"""Fit native straight-fiber survival with a conditional multinomial likelihood.
Nested plane counts share events: they MUST NOT be fitted as independent bins.
The >=1 m fit isolates a long attenuation component; no photon time cut is used.
"""
import argparse,csv,hashlib,json,math
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def read(path):
    rows=list(csv.DictReader(path.open()))
    z=[float(r['distance_mm'])/1000 for r in rows]
    n=[int(r['first_fiber_crossings']) for r in rows]
    generated={int(r['generated']) for r in rows}
    if len(generated)!=1 or any(b>a for a,b in zip(n,n[1:])) or any(b<=a for a,b in zip(z,z[1:])):
        raise ValueError('Invalid/non-nested transmission counts')
    N=generated.pop()
    if not all(0<=x<=N for x in n):raise ValueError('Invalid denominator')
    return np.array(z),np.array(n),N


def fit(z,n,start=1):
    take=z>=start;zs=z[take]-start;ns=n[take];ds=ns[:-1]-ns[1:]
    if ns[0]<50:raise ValueError('Insufficient photons at fit start')
    def nll(rate):
        logp=-rate*zs[:-1]+np.log(-np.expm1(-rate*np.diff(zs)))
        return -float(np.dot(ds,logp))+int(ns[-1])*rate*float(zs[-1])
    lo,hi=.005,5.
    for _ in range(100):
        x=lo+(hi-lo)/3;y=hi-(hi-lo)/3
        if nll(x)<nll(y):hi=y
        else:lo=x
    rate=(lo+hi)/2;h=rate*1e-3;curvature=(nll(rate+h)-2*nll(rate)+nll(rate-h))/h**2
    if curvature<=0:raise ValueError('Invalid likelihood curvature')
    error=1/math.sqrt(curvature)
    probs=np.r_[np.exp(-rate*zs[:-1])*(-np.expm1(-rate*np.diff(zs))),np.exp(-rate*zs[-1])]
    obs=np.r_[ds,ns[-1]];expected=ns[0]*probs
    dev=2*sum(o*math.log(o/e) for o,e in zip(obs,expected) if o>0)
    return {'length_m':1/rate,'standard_error_m':error/rate**2,'rate_per_m':rate,
            'rate_standard_error':error,'fit_start_m':start,'fit_end_m':float(z[-1]),
            'photons_at_start':int(ns[0]),'conditional_multinomial_deviance':float(dev),
            'degrees_of_freedom':len(obs)-2,'normalization_at_start':int(ns[0]),
            'method':'Conditional multinomial ML of first-crossing survival; nested counts; single long component; curvature errors'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
    a.output_dir.mkdir(parents=True,exist_ok=True)
    target=a.output_dir/'attenuation_summary.json'
    if target.exists():raise ValueError('Refusing to overwrite '+str(target))
    result={'ordinary_path_length_m':4,'time_cut':None,'source':'Uniform core area, isotropic emission conditioned on forward meridional guided cone, full profile emission spectrum, single cladding, 1 mm outer diameter, 2% D radial PMMA, air outside; full native WLS reabsorptions',
            'reference':'Kuraray meeting 2026-11-13 page 8, dose zero; approximate visual readings; page 7 lower bounds',
            'acceptance_note':'20% agreement with graph proxies is an explicit engineering tolerance, NOT experimental uncertainty or exact protocol reproduction',
            'scope':'Validation of the APPROXIMATE common residual-loss model. Spectral transmission and independent PS/PMMA absorption are not validated.',
            'profiles':{},'inputs':{}}
    fig,ax=plt.subplots(figsize=(9,6),dpi=200)
    for short,name,ref,lower,color in [('y11','Y-11',3.8,3.5,'black'),('ys6','YS-6',3.8,3.,'tab:blue'),('ys4','YS-4',3.4,3.,'tab:orange')]:
        path=a.input_dir/f'straight-{short}.csv';z,n,N=read(path);f=fit(z,n)
        f.update({'reference_graph_m':ref,'catalogue_lower_bound_m':lower,'relative_difference_from_graph':f['length_m']/ref-1,
                  'within_20_percent_graph_proxy':abs(f['length_m']/ref-1)<.2,'above_catalogue_bound_95pct_stat':f['length_m']-1.96*f['standard_error_m']>lower})
        f['passed']=f['within_20_percent_graph_proxy'] and f['above_catalogue_bound_95pct_stat']
        result['profiles'][name]=f;result['inputs'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        probability=n/N;ax.errorbar(z,probability,np.sqrt(probability*(1-probability)/N),fmt='o',color=color,ms=3,capsize=2,label=f"{name}: L = {f['length_m']:.2f} +/- {f['standard_error_m']:.2f} m")
        xx=np.linspace(1,6,200);ax.plot(xx,f['normalization_at_start']/N*np.exp(-(xx-1)/f['length_m']),color=color)
    mono=a.input_dir/'straight-mono600.csv';z,n,N=read(mono);mu=(np.arange(20000)+.5)/20000*(1-1.49/1.59)+1.49/1.59
    predicted=np.array([np.mean(np.exp(-(x-.00001)/(4*mu))) for x in z]);observed=n/N
    residual=np.abs(observed-predicted);bounds=6*np.sqrt(predicted*(1-predicted)/N)+.002
    result['monochromatic_600nm']={'passed':bool(np.all(residual<bounds)),'generated':N,'maximum_absolute_probability_residual':float(max(residual)),
        'analytic_survival':'mean_mu exp[-(z-0.01 mm)/(4000 mm * mu)], mu uniform [1.49/1.59,1]','acceptance':'6 binomial standard deviations + 0.002 absolute probability',
        'observed':observed.tolist(),'predicted':predicted.tolist()}
    result['inputs'][mono.name]=hashlib.sha256(mono.read_bytes()).hexdigest()
    result['passed']=all(x['passed'] for x in result['profiles'].values()) and result['monochromatic_600nm']['passed']
    ax.set(xlim=(0,6.2),ylim=(0,1),xlabel='Axial distance from source [m]',ylabel='First-crossing probability per launched photon',title='Straight Kuraray WLS fibers: effective attenuation')
    ax.grid(alpha=.25);ax.legend();fig.text(.5,.01,'Geant4 10.7.4 | common ordinary ray-path loss: 4 m | long-component fit: 1-6 m | no time cut',ha='center',fontsize=8);fig.tight_layout(rect=(0,.03,1,1));fig.savefig(a.output_dir/'straight_fiber_attenuation.png');plt.close(fig)
    target.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    for name,r in result['profiles'].items():print(f"{name}: L = {r['length_m']:.4f} +/- {r['standard_error_m']:.4f} m; PASS={r['passed']}")
    print('MONOCHROMATIC_PASS',result['monochromatic_600nm']['passed'])
    if not result['passed']:raise SystemExit('Attenuation compatibility check FAILED')
if __name__=='__main__':main()
