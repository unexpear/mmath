import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt,re
L=open('glk_out.txt').read().split('\n');rows=[]
for i in range(0,12,2):
  r=L[i];m=L[i+1];nm=r.split('->')[0];f=r.split('->')[1].split('Hz')[0]
  g=lambda s,k:re.search(k+r' +([\d.]+)',s).group(1)
  pk=lambda s:' '.join(s.split('peaks ')[1].split()[:3])
  rows.append((f'{nm} ({f} Hz)',g(r,'cen')+' / '+g(m,'cen'),g(r,'T60')+' / '+g(m,'T60'),pk(r),pk(m)))
fig,ax=plt.subplots(figsize=(14,4.6),dpi=100);ax.axis('off')
t=ax.table(cellText=[list(x) for x in rows],colLabels=['VSCO note (sounding)','Centroid ref / model Hz','f1 T60 ref / model s','REF partials ratio(dB)','MODEL partials ratio(dB)'],loc='center',cellLoc='left',colWidths=[.15,.17,.15,.27,.27])
t.auto_set_font_size(False);t.set_fontsize(10.5);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 - glockenspiel (struck_bar.h + glockenspiel.h) vs VSCO 2 CE glock_medium\nref f1 sits +17 c (A4 = 444?); low bars measure mode 2 at 2.90-3.05 vs uniform-bar 2.72-2.74 (VERIFY geometry)',fontsize=12)
fig.savefig('/workspace/glock-scoreboard1.png',bbox_inches='tight')
