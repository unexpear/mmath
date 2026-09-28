"""Capture native desktop screenshots and SDL's final device output."""
import json
import argparse
import os
from pathlib import Path
import subprocess
import numpy as np

root=Path(__file__).resolve().parents[2]
out=root/'build_real_audio/ui-audio'
out.mkdir(exist_ok=True)
parser=argparse.ArgumentParser()
parser.add_argument('--exe',type=Path,default=root/'build_app_msvc/verified/mmathen_app.exe')
args=parser.parse_args()
exe=args.exe.resolve()
sample=root/'build_real_audio/reference_sources/f3-emily-guitar/a2.wav'
reports=[]
for name,args in [('tuner',['--tuner',str(sample)]),('calibration',['--tuner','--calibration',str(sample)]),('muted',['--tuner','--mute',str(sample)]),('analyzer',[str(sample)]),('reference',['--reference-demo',str(root/'build_real_audio/reference-suite.tsv')]),('reference-public',['--reference-demo',str(root/'build_real_audio/public-domain-suite.tsv')])]:
    raw=out/(name+'.raw')
    env=dict(os.environ,SDL_AUDIO_DRIVER='disk',SDL_AUDIO_DISK_OUTPUT_FILE=str(raw))
    p=subprocess.run([str(exe),'--capture',str(out/(name+'.png')),'--seconds','2',*args],cwd=exe.parent,env=env,capture_output=True,text=True,timeout=120)
    (out/(name+'.log')).write_text(p.stdout+p.stderr)
    if p.returncode: raise RuntimeError(name+': '+p.stdout+p.stderr)
    # Device spec is saved by the app, after opening the actual SDL device.
    stats=dict(line.split('=',1) for line in (out/(name+'.png.txt')).read_text().splitlines() if '=' in line)
    if int(stats['outputFormat'])!=0x8120: raise ValueError('Expected SDL F32LE; inspect device format')
    x=np.fromfile(raw,dtype='<f4')
    report=dict(name=name,samples=len(x),finite=bool(np.isfinite(x).all()),peak=float(np.abs(x).max()),rms=float(np.sqrt(np.mean(x.astype(float)**2))),stats=stats)
    if not report['finite'] or report['peak']>1: raise ValueError('Invalid output')
    if name=='muted': assert report['peak']==0
    else: assert report['rms']>1e-5
    if name.startswith('reference'):
        assert stats['referenceReady']=='1' and not stats['referenceError']
        assert stats['referenceEngine']=='MMathEn'
    reports.append(report)
    print(json.dumps({k:v for k,v in report.items() if k!='stats'}),flush=True)
(out/'report.json').write_text(json.dumps(reports,indent=2)+'\n')
