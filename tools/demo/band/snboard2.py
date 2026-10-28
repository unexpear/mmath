import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
rows=[('Step (oversample)','8x: not converged','16x (= 48x within 1 dB)','-'),
('Centroid 1 / 3 / 6 m/s','1094 / 1540 / 1750 Hz','973 / 1775 / 2219 Hz','1094-1542 (v3-v7)'),
('Late >2 kHz 1 / 3 / 6 m/s','-6.3 / -8.8 / -7.9 dB (8x buzz)','-25 / -15 / -8.4 dB','-8.3..-9.3 dB'),
('Drum peak, snares on','212-220 Hz','232-236 Hz','236-244 Hz'),
('Drum peak, snares off','192 Hz','208 Hz','220 Hz'),
('Snares off t-20 / t-40','367 / 840 ms','114 / 262 ms','146 / 282-290 ms'),
('Snares on t-20 / t-40','19-28 / 72-105 ms','18-22 / 57-78 ms','33-56 / 157-200 ms (room)'),
('Crest 300 ms 1 / 3 / 6 m/s','26.9 / 27.2 / 29.0 dB','20.4 / 22.5 / 25.4 dB','17.1-18.9 dB'),
('Crest first 10 ms (3 m/s)','13.7 dB','(coherent wire slap at +7 ms)','5.2-5.8 dB')]
fig,ax=plt.subplots(figsize=(13,5.4),dpi=100);ax.axis('off')
t=ax.table(cellText=[list(r) for r in rows],colLabels=['Check','Before (7c77c2f)','Now','VSCO 2 CE Snare2'],loc='center',cellLoc='left',colWidths=[.25,.25,.25,.25])
t.auto_set_font_size(False);t.set_fontsize(11);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 - snare fixes: convergence, pitch, snares-off damping, stick click',fontsize=13)
fig.savefig('/workspace/snare-scoreboard2.png',bbox_inches='tight')
