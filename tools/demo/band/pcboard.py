import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
rows=[('Cello low attack 10-90% (C2-B2)','561 ms (C2 951)','~190 ms (C2 408)','565 ms*'),
('Cello pitch, median (fine FFT)','+5 c (pyin artifact) ','+0.4 c (all within 2.3)','0 c'),
('Cello deep centroid/f0','-','4.25','3.42'),
('Soft flute H2 / H3 (p)','-17 / -19 dB','-22 / -21 dB','-23 / -26 dB'),
('Guitar LTAS mean |dev|','8.0 dB','3.2 dB','0'),
('Guitar E2 vs B4 balance','E2 20 dB low','within LTAS','-'),
('Snare centroid 1/3/6 m/s','(new)','1094 / 1540 / 1750 Hz','1094-1542'),
('Snare late >2k','(new)','-6.3 / -8.8 / -7.9 dB','-9 dB'),
('Snare t-20 / t-40','(new)','19-28 / 72-105 ms','33-56 / 157-200 (room)'),
('Snare drum peak','(new)','212-220 Hz','236-244 Hz'),
('Snares off t-40','(new)','~800 ms','~290 ms')]
fig,ax=plt.subplots(figsize=(13,6.4),dpi=100);ax.axis('off');fig.patch.set_facecolor('w')
t=ax.table(cellText=[list(r) for r in rows],colLabels=['Check','Before','Now (7c77c2f)','Reference'],loc='center',cellLoc='left',colWidths=[.34,.2,.25,.21])
t.auto_set_font_size(False);t.set_fontsize(11);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 - pro checks: deep cello, soft flute, guitar, snare\n* ref median includes expressive swells; refs: PD Sarabande, VSCO 2 CE (CC0), Greeninger (CC0)',fontsize=12)
fig.savefig('/workspace/prochecks-scoreboard1.png',bbox_inches='tight')
