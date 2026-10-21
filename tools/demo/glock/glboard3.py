import subprocess,re
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
def rows(f):
  o=subprocess.run(['python3','glk.py',f],capture_output=True,text=True).stdout.splitlines();R=[];M=[];names=[]
  for l in o:
    c=re.search(r'cen\s+(\d+)',l).group(1);pk=[(float(a),int(b)) for a,b in re.findall(r'([\d.]+)\((-?\d+)\)',l.split('peaks')[1])]
    def lev(lo,hi):
      v=[b for a,b in pk if lo<a<hi];return str(max(v)) if v else '<-40'
    t=f"{c} Hz | {lev(0.9,1.1)} / {lev(2.5,3.2)} / {lev(5.0,6.0)}"
    if 'REF' in l: names.append(l.split('->')[0]);R.append(t)
    else: M.append(t)
  return names,R,M
nm,R,A=rows('gD.f64');_,_,D=rows('gE.f64')
lab=['G5','C6','G6','C7','G7','C8']
cell=[[f"{lab[i]} (file {nm[i]})",R[i],A[i],D[i]] for i in range(6)]
fig,ax=plt.subplots(figsize=(14,4.2),dpi=100);ax.axis('off')
t=ax.table(cellText=cell,colLabels=['Bar (sounding)','REF centroid | mode 1 / 2 / 3 dB','MODEL before','MODEL now'],loc='center',cellLoc='left')
t.auto_set_font_size(False);t.set_fontsize(10.5);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 glockenspiel vs VSCO 2 CE glock_medium (CC0): free bars vs case baffle 0.3 (grid error 114 -> 115)',fontsize=12)
fig.savefig('/workspace/glock-scoreboard3.png',bbox_inches='tight')
