import subprocess,sys,math
tags=sys.argv[1].split('+')
out=subprocess.run(['python3','/workspace/r/tools/demo/winds_pro1/wpscore.py','x.png','+'.join(tags),'trumpet','/workspace/r/tests/wind_excerpts.h'],capture_output=True,text=True).stdout.splitlines()
rows={l.rsplit(' | ',len(tags)+1)[0].strip():[float(v) for v in l.rsplit(' | ',len(tags)+1)[1:]] for l in out if l.count(' | ')>=len(tags)+1}
keys=['Band 0-1 kHz (dB)','Band 1-3 kHz (dB)','Band 3-8 kHz (dB)','Band 8-16 kHz (dB)','H2 re H1 (dB)','H3 re H1 (dB)','H4 re H1 (dB)','H6 re H1 (dB)','Inter-harmonic floor (dB)']
for j,t in enumerate(tags):
  e=sum(abs(rows[k][j+1]-rows[k][0]) for k in keys)+abs(20*math.log10(rows['Spectral centroid (Hz)'][j+1]/rows['Spectral centroid (Hz)'][0]))
  print(t,round(e,1),'pitch',rows['Pitch |error| median (cents)'][j+1],rows['Pitch worst note (cents)'][j+1])
