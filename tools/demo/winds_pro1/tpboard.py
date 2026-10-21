import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
L=[l.split('|',1) for l in open('lick_table.txt') if l[:2] in ('G4','A4','A#','C5','F4')]
rows=[]
for nm,rest in L:
  parts=[p.split() for p in rest.split('||')]
  rows.append([nm.strip()]+[f"{p[0]} / {p[1]} / {p[2]}" for p in parts])
J=[l.split('||') for l in open('jb_table.txt') if l[:1] in 'DEFGABC' and '||' in l]
jr=[[j[0].split()[0],j[0].split()[1]]+[" / ".join(x.split()[:3])+f"  ({x.split()[3]})" for x in j[1:]] for j in J]
summ=[l.strip() for l in open('jb_table.txt') if l.startswith(('jbold:','jF:'))]
fig=plt.figure(figsize=(16,9),facecolor='#111');fig.text(0.02,0.95,'Trumpet pitch + attack fix: lip Q 85 -> 4 (Newton et al. 2008), lip ratios re-set, rows retuned',color='w',fontsize=18,weight='bold')
fig.text(0.02,0.91,'YIN, 1-cent. cells = steady cents / attack cents (15-60 ms) / settle ms (within 25 c for 30 ms)',color='#ccc',fontsize=12)
ax=fig.add_axes([0.02,0.47,0.96,0.42]);ax.axis('off')
t=ax.table(cellText=rows,colLabels=['Lick note','Lick Trumpet (real)','old default','new default'],loc='upper left',cellLoc='center');t.auto_set_font_size(False);t.set_fontsize(12);t.scale(1,1.6)
ax2=fig.add_axes([0.02,0.06,0.96,0.40]);ax2.axis('off')
t2=ax2.table(cellText=jr,colLabels=['JB note','n','old: steady/attack/settle (bad)','new: steady/attack/settle (bad)'],loc='upper left',cellLoc='center');t2.auto_set_font_size(False);t2.set_fontsize(11);t2.scale(1,1.25)
for tb in (t,t2):
  for (r,c),cell in tb.get_celld().items():
    cell.set_facecolor('#222' if r else '#335');cell.get_text().set_color('w');cell.set_edgecolor('#555')
fig.text(0.02,0.025,'Jingle Bells (medians per note): attacks off by > 50 c  85/98 -> 7/98;  settle median 65 -> 28 ms;  steady mean |c| 34.9 -> 2.3.   Lick steady worst -13.7 -> -3.8 c',color='#ccc',fontsize=12)
fig.savefig('/workspace/trumpet-pitch-scoreboard1.png',dpi=120,facecolor=fig.get_facecolor())
