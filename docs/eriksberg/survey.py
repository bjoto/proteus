"""Recreate planning maps from the retained survey, not a runtime asset converter.
Run with matplotlib and pyproj installed: python docs/eriksberg/survey.py
"""
import gzip
import hashlib
import json
import math
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle, Patch
from pyproj import Transformer

BASE = Path(__file__).resolve().parent
raw = gzip.decompress((BASE / 'osm-survey.json.gz').read_bytes())
data = json.loads(raw)
es = data['elements']
transform = Transformer.from_crs(4326, 3007, always_xy=True)
origin = transform.transform(11.9089495, 57.6973921)

def coords(g):
    return [(x - origin[0], y - origin[1]) for p in g if p and 'lat' in p
            for x, y in [transform.transform(p['lon'], p['lat'])]]

def join(groups):
    groups = [list(g) for g in groups if g]
    while True:
        found = False
        for i, a in enumerate(groups):
            for j in range(i + 1, len(groups)):
                b = groups[j]
                if a[-1] == b[0]: c = a + b[1:]
                elif a[-1] == b[-1]: c = a + list(reversed(b))[1:]
                elif a[0] == b[-1]: c = b + a[1:]
                elif a[0] == b[0]: c = list(reversed(b)) + a[1:]
                else: continue
                groups[i] = c
                groups.pop(j)
                found = True
                break
            if found: break
        if not found: return groups

def outlines(e):
    if e['type'] == 'way': return [coords(e.get('geometry', []))]
    return join([coords(m.get('geometry', [])) for m in e.get('members', [])
                 if m.get('role') == 'outer'])

buildings = [e for e in es if 'building' in e.get('tags', {})]
member_ids = {m['ref'] for e in buildings if e['type'] == 'relation'
              for m in e['members'] if m['type'] == 'way'}
buildings = [e for e in buildings if not(e['type'] == 'way' and e['id'] in member_ids)
             and e['tags'].get('man_made') != 'crane']
river = next(e for e in es if e['id'] == 3329487 and e['type'] == 'relation')
chains = sorted(join([coords(m.get('geometry', [])) for m in river['members']]), key=len, reverse=True)
a, b = chains[:2]
if math.dist(a[-1], b[0]) > math.dist(a[-1], b[-1]): b = list(reversed(b))
# Close the two local bank chains outside the displayed crop. This is a survey
# visualization of a clipped relation, not a complete water multipolygon export.
water = a + b
crane = next(e for e in es if e['id'] == 15956173 and e['type'] == 'relation')
crane_outline = outlines(crane)[0]
core = (-180, -180, 260, 350)
context = (-400, -400, 550, 500)
routes = [
    {'id': 'A', 'name': 'Street to river', 'color': '#c25b1b',
     'points': [(-85,170),(-78,90),(-70,0),(-30,-130),(120,-200)],
     'heights': [14,18,25,32,38], 'targets': [(-20,85),(0,0),(0,0)],
     'caption': 'Recommended: close foreground, then open water'},
    {'id': 'B', 'name': 'Dock-axis flight', 'color': '#7c3cb1',
     'points': [(-10,280),(0,155),(15,50),(25,-90),(90,-200)],
     'heights': [18,22,24,28,38], 'targets': [(0,0),(0,0),(0,0)],
     'caption': 'Strong symmetry; bridge and crane clearance unresolved'},
    {'id': 'C', 'name': 'River arc', 'color': '#087e82',
     'points': [(250,-40),(200,-125),(100,-200),(-40,-245),(-180,-230)],
     'heights': [18,24,32,40,48], 'targets': [(0,0),(0,0),(0,0)],
     'caption': 'Open silhouette study; weakest street-to-water reveal'},
]

def draw_base(ax, limits):
    ax.set_facecolor('#eeeae1')
    ax.add_patch(Polygon(water, facecolor='#c5e2ee', edgecolor='#6297aa', lw=.8))
    for e in es:
        if 'highway' in e.get('tags', {}):
            g = coords(e.get('geometry', []))
            if len(g) > 1:
                x, y = zip(*g)
                ax.plot(x, y, color='#b5a995', lw=.55, zorder=2)
    for e in buildings:
        tags = e['tags']
        color = '#748697' if 'height' in tags or 'building:levels' in tags else '#bfc0bd'
        for g in outlines(e):
            if len(g) >= 3:
                ax.add_patch(Polygon(g, facecolor=color, edgecolor='#f7f5ef', lw=.35, zorder=3))
    ax.add_patch(Polygon(crane_outline, facecolor='#d34335', edgecolor='#982d22', lw=.7, zorder=5))
    ax.text(5,-25,'CRANE', fontsize=8, weight='bold', zorder=8)
    ax.text(8,215,'Dock basin', rotation=90, ha='center', color='#386779', fontsize=8)
    ax.text(220,-120,'Göta älv',color='#386779',fontsize=10,rotation=25)
    ax.text(70,130,'Dockbron',fontsize=7,color='#514b44')
    ax.annotate('',xy=(43,128),xytext=(76,127),arrowprops={'arrowstyle':'-','color':'#514b44'})
    ax.set_xlim(limits[0],limits[2]); ax.set_ylim(limits[1],limits[3]); ax.set_aspect('equal')
    ax.grid(alpha=.16);ax.tick_params(labelsize=7)
    ax.set_xlabel('Metres east of crane',fontsize=8);ax.set_ylabel('Metres north of crane',fontsize=8)
    ax.annotate('N',xy=(.95,.95),xytext=(.95,.86),xycoords='axes fraction',ha='center',
                fontsize=10,arrowprops={'arrowstyle':'->','color':'#333'})

def draw_route(ax, r):
    pts = r['points']; x,y=zip(*pts)
    ax.plot(x,y,color=r['color'],lw=2.7,zorder=7)
    for p,q in zip(pts,pts[1:]):
        ax.annotate('',xy=q,xytext=((p[0]+q[0])/2,(p[1]+q[1])/2),
                    arrowprops={'arrowstyle':'->','color':r['color'],'lw':2},zorder=7)
    for shot, index in enumerate([0,2,4],1):
        p=pts[index];target=r['targets'][shot-1]
        ax.plot([p[0],target[0]],[p[1],target[1]],color=r['color'],lw=.8,ls=':',alpha=.8,zorder=6)
        ax.scatter(*p,s=160,color=r['color'],edgecolor='white',zorder=8)
        ax.text(*p,str(shot),color='white',ha='center',va='center',fontsize=8,weight='bold',zorder=9)

plt.rcParams.update({'font.family':'DejaVu Sans'})
fig, axes=plt.subplots(1,3,figsize=(16,7))
for ax,r in zip(axes,routes):
    draw_base(ax,(-210,-280,330,340));draw_route(ax,r)
    ax.set_title(r['id']+' / '+r['name'],loc='left',fontsize=13,weight='bold',pad=12)
    ax.text(0,-.13,r['caption'],transform=ax.transAxes,fontsize=8,wrap=True)
fig.suptitle('ERIKSBERG / Three camera approaches',x=.055,ha='left',fontsize=20,weight='bold')
fig.text(.055,.045,'Numbered points = approach / reveal / departure. Dotted lines = horizontal viewing direction.\n'
         'Concept paths, not collision-checked splines. North up; geographic footprints, no inferred building heights drawn.',fontsize=9)
fig.text(.055,.012,'Map data © OpenStreetMap contributors, ODbL · https://www.openstreetmap.org/copyright · Snapshot 2026-09-09',fontsize=8)
fig.subplots_adjust(left=.055,right=.98,top=.87,bottom=.20,wspace=.24)
fig.savefig(BASE/'camera-approaches.png',dpi=150);fig.savefig(BASE/'camera-approaches.svg')
plt.close(fig)
fig,ax=plt.subplots(figsize=(9,9));draw_base(ax,context)
ax.add_patch(Rectangle((core[0],core[1]),core[2]-core[0],core[3]-core[1],fill=False,
                       edgecolor='#b66324',lw=2,ls='--',zorder=6))
ax.text(-175,360,'Detailed geometry crop: 440 × 530 m',color='#994719',fontsize=9)
draw_route(ax,routes[0]);ax.set_title('Eriksberg / Coverage and recommended approach',loc='left',pad=16)
ax.legend(handles=[Patch(facecolor='#748697',label='Building with height or floor-count tag'),
                   Patch(facecolor='#bfc0bd',label='Building without either tag'),
                   Patch(facecolor='#d34335',label='Crane footprint — model separately')],
          loc='lower left',fontsize=8,framealpha=.95)
fig.text(.12,.035,'Outer view: 950 × 900 m context crop. Counts use building bounding-box centres;\n'
         'building relation members deduplicated, crane and building:part-only features excluded.',fontsize=8)
fig.text(.12,.012,'© OpenStreetMap contributors, ODbL · https://www.openstreetmap.org/copyright · 2026-09-09',fontsize=8)
fig.subplots_adjust(bottom=.12,top=.94)
fig.savefig(BASE/'coverage.png',dpi=150);fig.savefig(BASE/'coverage.svg');plt.close(fig)
summary={'snapshot':data['osm3s']['timestamp_osm_base'],'sha256':hashlib.sha256(raw).hexdigest(),
         'origin_epsg3007':origin,'crane_osm_relation':15956173,'crops':{},'routes':routes}
inv=Transformer.from_crs(3007,4326,always_xy=True)
for name,bounds in [('core',core),('context',context)]:
    subset=[]
    for e in buildings:
        g=[p for outline in outlines(e) for p in outline]
        if not g:continue
        x=(min(p[0]for p in g)+max(p[0]for p in g))/2
        y=(min(p[1]for p in g)+max(p[1]for p in g))/2
        if bounds[0]<=x<=bounds[2] and bounds[1]<=y<=bounds[3]:subset.append(e)
    summary['crops'][name]={'local_bounds':bounds,'buildings':len(subset),
      'height':sum('height'in e['tags']for e in subset),
      'floors':sum('building:levels'in e['tags']for e in subset),
      'either':sum('height'in e['tags']or 'building:levels'in e['tags']for e in subset),
      'roof_shape':sum('roof:shape'in e['tags']for e in subset),
      'sw_lon_lat':inv.transform(origin[0]+bounds[0],origin[1]+bounds[1]),
      'ne_lon_lat':inv.transform(origin[0]+bounds[2],origin[1]+bounds[3])}
for r in routes:
    r['length_m']=round(sum(math.dist(p,q)for p,q in zip(r['points'],r['points'][1:])),1)
    r['waypoints_lon_lat']=[inv.transform(origin[0]+x,origin[1]+y)for x,y in r['points']]
(BASE/'survey-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary['crops'],indent=2))
print([(r['id'],r['length_m']) for r in routes])

# Matplotlib emits trailing spaces in SVG paths; normalize the review artifacts.
for name in ("camera-approaches.svg", "coverage.svg"):
    path = BASE / name
    path.write_text("\n".join(line.rstrip() for line in path.read_text().splitlines()) + "\n")
