import numpy as np
from PIL import Image
from scipy.optimize import least_squares
im=np.array(Image.open('tl.png').convert('L'))
def f_of(x): return 270*np.exp((x-213)/297.0)
def db_of(y): return -10-(y-66)/14.9
pts=[]
for x in range(130,560):
    col=im[70:690,x]; rows=np.where(col<110)[0]+70
    if len(rows)<3: continue
    # drop isolated grid dots: keep rows whose neighbors also dark in adjacent columns
    rr=[r for r in rows if (im[r,x-1]<160 or im[r,x+1]<160 or im[r-1,x]<160 or im[r+1,x]<160)]
    if len(rr)<3: continue
    pts.append((f_of(x),db_of(np.median(rr)),db_of(min(rr)),db_of(max(rr))))
P=np.array(pts); np.savetxt('saitis2012_fig2tl_digitized.csv',P,delimiter=',',header='freq_Hz,median_dB,upper_dB,lower_dB',fmt='%.2f',comments='')
f=P[:,0];y=P[:,1]
sel=(f>=220)&(f<=640);f=f[sel];y=y[sel]
w=2*np.pi*f
def Y(p):
    fA0,fC,fBm,fBp,lmA0,lmC,lmBm,lmBp,QC=p
    tot=0
    for fk,lm,Q in [(fA0,lmA0,20),(fC,lmC,QC),(fBm,lmBm,41),(fBp,lmBp,43)]:
        wk=2*np.pi*fk; tot=tot+1j*w/(np.exp(lm)*(wk**2-w**2+1j*w*wk/Q))
    return tot
def res(p): return 20*np.log10(np.abs(Y(p)))-y
p0=[275,400,470,541,np.log(0.1),np.log(0.3),np.log(0.1),np.log(0.1),30]
lb=[255,360,440,505,np.log(1e-3)]*1+[np.log(1e-3)]*3+[5];ub=[295,440,505,580,np.log(10)]+[np.log(10)]*3+[100]
r=least_squares(res,p0,bounds=(lb,ub))
p=r.x;e=res(p)
print('rms dB',np.sqrt(np.mean(e**2)),'max',np.abs(e).max(),'n',len(e))
for nm,fk,lm,Q in zip(['A0','CBR','B1-','B1+'],p[:4],p[4:8],[20,p[8],41,43]): print(nm,round(fk,1),'Hz m',round(np.exp(lm)*1e3,1),'g Q',round(Q,1))
# spread of the 10 curves (upper-lower) as digitizing+population spread
print('median spread dB (upper-lower) in band',np.median(P[sel,2]-P[sel,3]))
J=r.jac;cov=np.linalg.pinv(J.T@J)*np.mean(e**2);print('1-sigma rel mass',[round(float(np.sqrt(cov[i,i])),2) for i in range(4,8)])
