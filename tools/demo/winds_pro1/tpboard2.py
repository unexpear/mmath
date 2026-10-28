import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
L=[l.split('|',1) for l in open('lick_table2.txt') if l[:2] in ('G4','A4','A#','C5','F4')]
rows=[]
for nm,rest in L:
  parts=[p.split() for p in rest.split('||')]
  rows.append([nm.strip()]+[f"{p[0]} / {p[1]} / {p[2]}" for p in parts])
J=[l.split('||') for l in open('jb_table2.txt') if l[:1] in 'DEFGABC' and '||' in l]
jr=[[j[0].split()[0],j[0].split()[1]]+[" / ".join(x.split()[:3])+f"  ({x.split()[3]})" for x in j[1:]] for j in J]
summ=[l.strip() for l in open('jb_table2.txt') if l.startswith(('jF:','jI:'))]
fig=plt.figure(figsize=(16,9),facecolor='#111');fig.text(0.02,0.95,'Trumpet 9d9ccdd -> 9460ef0: per-note pressure (Fletcher & Tarnopolsky 1999) + 2-section wall loss',color='w',fontsize=18,weight='bold')
fig.text(0.02,0.91,'YIN, 1-cent. cells = steady cents / attack cents (15-60 ms) / settle ms (within 25 c for 30 ms)',color='#ccc',fontsize=12)
ax=fig.add_axes([0.02,0.47,0.96,0.42]);ax.axis('off')
t=ax.table(cellText=rows,colLabels=['Lick note','Lick Trumpet (real)','9d9ccdd','9460ef0'],loc='upper left',cellLoc='center');t.auto_set_font_size(False);t.set_fontsize(12);t.scale(1,1.6)
ax2=fig.add_axes([0.02,0.06,0.96,0.40]);ax2.axis('off')
t2=ax2.table(cellText=jr,colLabels=['JB note','n','9d9ccdd: steady/attack/settle (bad)','9460ef0: steady/attack/settle (bad)'],loc='upper left',cellLoc='center');t2.auto_set_font_size(False);t2.set_fontsize(11);t2.scale(1,1.25)
for tb in (t,t2):
  for (r,c),cell in tb.get_celld().items():
    cell.set_facecolor('#222' if r else '#335');cell.get_text().set_color('w');cell.set_edgecolor('#555')
fig.text(0.02,0.025,'JB attacks > 50 c 7/98 -> 7/98; JB steady mean |c| 2.3 -> 1.6; Lick steady worst -3.8 -> +0.2 c; centroid 756 -> 1115 Hz (real 1393); tscore 63.4 -> 50.7 (lower = closer)',color='#ccc',fontsize=11)
fig.savefig('/workspace/trumpet-pitch-scoreboard2.png',dpi=120,facecolor=fig.get_facecolor())
