import sys,numpy as n
def rel(f):
  x=n.fromfile(f);fs=48000;h=240;e=20*n.log10(n.sqrt(n.convolve(x**2,n.ones(h)/h,'same'))[::h]+1e-12);t=n.arange(len(e))*0.005
  T=len(x)/fs-0.5;m=n.median(e[(t>T-0.4)&(t<T-0.1)]);seg=(t>T-0.1)
  ee=e[seg];tt=t[seg];i10=n.argmax(ee<m-10);i30=n.argmax(ee<m-30)
  k=n.where((ee<m-3)&(ee>m-23))[0]
  rate=n.polyfit(tt[k],ee[k],1)[0] if len(k)>3 else n.nan
  return (tt[i10]-T)*1000,(tt[i30]-T)*1000,rate
for r in sys.argv[1:]:
  for i in ['clarinet','saxophone']:
    print(r,i,'to -10 dB %.0f ms, to -30 dB %.0f ms, -3..-23 dB rate %.0f dB/s'%rel(r+'_'+i+'.f64'))
