import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
rows=[('Source','La Monnoye 1720 tune; Charles Wood SATB (PD), hymnsandcarolsofchristmas MIDI','-'),
('Form / tempo','1-bar snare intro + 12-bar verse x2, q = 140, 44.4 s','-'),
('Flute pitch, median |err| (pyin, 10 c bins)','5.0 c, 64 notes, 0 octave errors','0'),
('Cello pitch, median |err|','1.3 c, 68 notes, 0 octave errors','0'),
('Notes rendered fl / vc / gt / sd','93 / 100 / 214 / 115, 0 fail','-'),
('Stem RMS fl / vc / gt / sd','-24 / -28 / -31 / -39 dBFS (targets 0/-4/-7/-15 rel.)','-'),
('Snare hit crest, 300 ms','27-29 dB','17.5-19 dB (VSCO)'),
('Snare 1 ms-RMS crest','17.9 dB (sub-ms wire click)','-'),
('Snare t-40','72-105 ms','157-200 ms (room)')]
fig,ax=plt.subplots(figsize=(13,5),dpi=100);ax.axis('off')
t=ax.table(cellText=[list(r) for r in rows],colLabels=['Check','Patapan band1','Reference'],loc='center',cellLoc='left',colWidths=[.33,.47,.2])
t.auto_set_font_size(False);t.set_fontsize(11);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 - Patapan band (flute, deep cello, classical guitar, snare)',fontsize=13)
fig.savefig('/workspace/patapan-scoreboard1.png',bbox_inches='tight')
