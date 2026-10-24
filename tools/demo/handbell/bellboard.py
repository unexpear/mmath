import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
rows=[('Celesta C5/D5/E5/G5 spectrum','fundamental only (>45 dB clear)','fundamental; 2f at -15..-44 dB'),
('Celesta f1 T60 C5/D5/E5/G5','4.6 / 4.1 / 3.7 / 3.1 s','4.5 / 3.9 / 6.7 / 4.1 s'),
('Chime C4 partials (Hz)','523 768 1053 1372 1722 2097 2493','525 777 1070 1402 1770 2168 2593'),
('Chime partial error C4 / G4 / C5 / F5','-0.4..-3.9 % / -0.3..-1.9 % / +0.5..-3.3 % / +2.9..-3.0 %','0'),
('Chime centroid C4/G4/C5/F5','2269 / 1822 / 1538 / 1323 Hz','1695 / 2018 / 1707 / 1553 Hz'),
('Chime T60 modes 4-5 C4 / F5','16 / 11 s ; 14 / 7.4 s','26 / 13 s ; 13 / 7.5 s'),
('Handbell F3 partials (dB)','1(0) 3.00(-3) 4.83(-27) 5.06(-20)','1(0) 1.99(-10) 3.01(-6) 4.85(-23) 6.12(-18)'),
('Handbell F3 / G6 f1 T60','18.7 / 6.2 s','19.2 / 5.8 s'),
('Handbell 2nd harmonic (F3)','about -40 dB (rim breathing, quadratic)','-10 dB  (GAP)'),
('Handbell G6 centroid','1.5 kHz','6.0 kHz (GAP: brighter clapper?)')]
fig,ax=plt.subplots(figsize=(14,5.6),dpi=100);ax.axis('off')
t=ax.table(cellText=[list(r) for r in rows],colLabels=['Check','Model','Reference (CC0)'],loc='center',cellLoc='left',colWidths=[.28,.38,.34])
t.auto_set_font_size(False);t.set_fontsize(10.5);t.scale(1,1.7)
for (i,j),c in t.get_celld().items():
  if i==0: c.set_facecolor('#223');c.get_text().set_color('w')
ax.set_title('mmath PR #2 - celesta, tubular bells, handbells vs CC0 refs (Freesound Macsat-Rd, VSCO 2 CE TB_hit, Freesound 557142 + radwoc)',fontsize=12)
fig.savefig('/workspace/bells-scoreboard1.png',bbox_inches='tight')
