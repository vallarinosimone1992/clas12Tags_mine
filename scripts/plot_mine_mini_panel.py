#!/usr/bin/env python3
"""Plot the CSV exported by mini_spiral_model_test; dimensions in millimetres."""
import argparse
import csv
from pathlib import Path
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle, Polygon

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('csv', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
paths = [[], []]
with a.csv.open() as f:
    for row in csv.DictReader(f):
        paths[int(row['fiber'])].append(tuple(float(row[k]) for k in ('x_mm','y_mm','z_mm')))
fig = plt.figure(figsize=(12,8), dpi=150, layout='constrained')
gs = fig.add_gridspec(2,2,width_ratios=[1.15,1])
ax = fig.add_subplot(gs[:,0])
ax.add_patch(Rectangle((-150,-250),300,500,facecolor='#eeeeee',edgecolor='black',lw=1.2))
x,y,z = zip(*paths[0])
ax.plot(x,y,color='#2568a6',lw=1,label='Two vertically stacked 1 mm fibers')
for n,(sx,sy) in enumerate(((130,-250),(100,-250))):
    ax.plot(sx,sy-1,'s',color=('#b3261e','#1b8048')[n],ms=5)
    ax.annotate(f'SiPM {n}',(sx,sy-1),xytext=(35-105*n,-287-12*n),fontsize=9,
                arrowprops={'arrowstyle':'-','color':'black'})
ax.set(xlabel='X [mm]',ylabel='Y [mm]',title='Mini panel: fiber layout (top view)',xlim=(-165,165),ylim=(-310,265))
ax.set_aspect('equal');ax.legend(loc='upper left',fontsize=8)
side = fig.add_subplot(gs[0,1])
side.add_patch(Rectangle((-2.3,1.8),4.6,2.2,facecolor='#d9e9f5',edgecolor='none'))
side.add_patch(Rectangle((-.5,2),1,2,facecolor='#ffead2',edgecolor='#946b40',lw=1))
for zz in (2.5,3.5):
    side.add_patch(Circle((0,zz),.5,facecolor='#cdd4df',edgecolor='black',lw=1))
    side.add_patch(Circle((0,zz),.47,facecolor='#77ad73',edgecolor='#397331',lw=.6))
side.axhline(4,color='black',lw=1);side.text(.65,3.2,'Gel',fontsize=9)
side.text(-2.15,2.2,'EJ-200',fontsize=9)
side.set(xlabel='Local transverse position [mm]',ylabel='Z from panel center [mm]',title='Spiral groove: 1 × 2 mm',xlim=(-2.3,2.3),ylim=(1.8,4.5))
side.set_aspect('equal',adjustable='box')
tent = fig.add_subplot(gs[1,1]);apex=5.5+.5/math.cos(math.pi/6);w=(apex-4)/math.tan(math.pi/6)
tent.add_patch(Rectangle((-4.3,3.5),8.6,.5,facecolor='#d9e9f5',edgecolor='none'))
tent.add_patch(Polygon([(-w,4),(0,apex),(w,4)],facecolor='#f5f5f5',edgecolor='#777777',lw=1.3))
for zz in (4.5,5.5):
    tent.add_patch(Circle((0,zz),.5,facecolor='#cdd4df',edgecolor='black',lw=1))
    tent.add_patch(Circle((0,zz),.47,facecolor='#77ad73',edgecolor='#397331',lw=.6))
tent.text(-3.5,4.2,'Air',fontsize=9);tent.text(1.8,5.3,'Mylar: 30°',fontsize=9)
tent.set(xlabel='Local transverse position [mm]',ylabel='Z from panel center [mm]',title='Raised S: tangent Mylar tent',xlim=(-4.3,4.3),ylim=(3.5,6.8))
tent.set_aspect('equal',adjustable='box')
fig.suptitle('MINE mini_panel_spiral — 300 × 500 × 8 mm EJ-200',fontsize=15)
fig.supxlabel('Sections show the horizontal part of each route; numerical clearances below 1 µm are omitted.',fontsize=8)
a.output.parent.mkdir(parents=True,exist_ok=True)
fig.savefig(a.output)
