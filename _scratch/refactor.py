import re, sys, io
T='tests/test_wind_tone.cpp'; E='tests/wind_excerpts.h'
src=io.open(T,encoding='utf-8',newline='').read(); CR='\r\n' in src; src=src.replace('\r\n','\n')
exc=io.open(E,encoding='utf-8',newline='').read().replace('\r\n','\n')
def block_end(s,i):
    # i at '{'
    d=0
    for j in range(i,len(s)):
        if s[j]=='{': d+=1
        elif s[j]=='}':
            d-=1
            if d==0: return j
    raise Exception('unbalanced')
def lam(s,name,start=0):
    i=s.index('auto %s = ['%name,start); b=s.index('{',s.index(')',i)); e=block_end(s,b)
    assert s[e+1]==';'; return i,e+2,s[b+1:e]
hdr=[]
# bells
a=src.index('// Bach Stradivarius 42 tenor trombone.'); i=src.index('void applyBach37Bell'); b=src.index('{',i); e=block_end(src,b)+1
bells=src[a:e]
sx=src.index('// Bach 42 large bore'); assert sx<i
bells=bells.replace('constexpr double','inline constexpr double').replace('void apply','inline void apply')
src=src[:a]+'// Bach 37 / Bach 42 bells, the .547 in bore: include/mmathen/instruments/wind_presets.h\n'+src[e:]
hdr.append(bells)
# clarinet + sax length tables (first occurrence canonical; all identical required)
def lift(name,fn):
    global src
    i,e,body=lam(src,name)
    first=body; out=[]
    pos=0
    while True:
        try: i,e,body=lam(src,name,pos)
        except ValueError: break
        if body!=first: print('DIFF',name,i); pos=e; continue
        rep='auto %s = [](double frequency) { return %s(frequency); };'%(name,fn)
        src=src[:i]+rep+src[e:]; pos=i+len(rep); out.append(i)
    print(name,'replaced',len(out))
    return 'inline double %s(double frequency) {%s}\n'%(fn,first)
hdr.append(lift('clarinetLen','clarinetSoundingLengthMeters'))
hdr.append(lift('saxLength','saxophoneSoundingLengthMeters'))
# brass finger blocks
pat=re.compile(r'    struct Finger \{\n      double hertz;\n      double length;\n      int mode;\n    \};\n')
blocks=[]
for m in list(pat.finditer(src)):
    s=m.start(); fi=src.index('auto find = [&](double hertz) {',s); b=src.index('{',fi+20); e=block_end(src,b)+2
    blocks.append((s,e,src[m.end():fi],src[b+1:e-2]))
print('finger blocks',len(blocks))
canon={}
for s,e,table,find in reversed(blocks):
    inst='Trumpet' if '0.26' in find else 'Trombone'
    canon.setdefault(inst,(table,find))
    if 'D5 for' in table: canon[inst]=(table,find)
    rep=('    using Finger = BrassFinger;\n    const auto& finger = k%sFingers;\n'
         '    auto find = [](double hertz) { return %sFinger(hertz); };'%(inst,inst.lower()))
    src=src[:s]+rep+src[e:]
hdr.append('struct BrassFinger {\n  double hertz;\n  double length;\n  int mode;\n};\n')
for inst,(table,find) in canon.items():
    t=table.replace('const Finger finger[] =','inline constexpr BrassFinger k%sFingers[] ='%inst)
    t='\n'.join(l[2:] if l.startswith('  ') else l for l in t.split('\n'))
    f=find.replace('Finger','BrassFinger').replace(': finger)',': k%sFingers)'%inst).replace('&finger[0]','&k%sFingers[0]'%inst)
    hdr.append(t+'inline BrassFinger %sFinger(double hertz) {%s}\n'%(inst.lower(),f))
# brass specs
ts='''    const double mass = 3.0e-5;
    const double lip0 = 0.75 * speed / 1.45;
    const double omega = constants::twoPi * lip0;
    TrumpetSpec spec{1.70, 1.09e-4, kAir, kTemp, 0.0, 8.0e-7, 1.5e-5, 1.2e-2, mass,
                     omega * omega * mass, 1.2e-5, 0.001};
'''
print('trumpet spec',src.count(ts)); src=src.replace(ts,'    TrumpetSpec spec = phraseTrumpetSpec(speed, kAir, kTemp);\n')
bs='''    const double mass = 6.0e-5;
    const double lip0 = 0.75 * speed / 2.75;
    const double omega = constants::twoPi * lip0;
    auto horn = unwrap(makeTrombone(TromboneSpec{3.05, 0.0, 3.0e-4, kAir, kTemp, 0.0, 1.0e-6,
                                                 2.5e-5, 1.5e-2, mass, omega * omega * mass,
                                                 2.0e-5, 0.0005}));
'''
print('trombone spec',src.count(bs)); src=src.replace(bs,'    auto horn = unwrap(makeTrombone(phraseTromboneSpec(speed, kAir, kTemp)));\n')
# turbulence / locked breath defaults
for old,new in [('reedTurbulence = 0.40;','reedTurbulence = kClarinetReedTurbulence;'),
                ('lockedBreathFraction = 0.013;','lockedBreathFraction = kClarinetLockedBreathFraction;'),
                ('reedTurbulence = 0.55;','reedTurbulence = kSaxophoneReedTurbulence;'),
                ('lockedBreathFraction = 0.0176;','lockedBreathFraction = kSaxophoneLockedBreathFraction;'),
                ('reedTurbulence = 0.56;','reedTurbulence = kFluteJetTurbulence;'),
                ('lockedBreathFraction = 0.063;','lockedBreathFraction = kFluteLockedBreathFraction;')]:
    print(old,src.count(old)); src=src.replace(old,new)
def brass(m):
    after=src[m.end():m.end()+400]
    return m.group(1)+('kTromboneReedTurbulence;' if ('Scott' in after or m.group(1).strip().startswith('bone')) else 'kTrumpetReedTurbulence;')
src=re.sub(r'(\w+\.reedTurbulence = )0\.20;',brass,src)
print('brass turb left',src.count('reedTurbulence = 0.20'))
# lead-in
leads=re.findall(r'inline const Note (k\w+)\[\] = \{\n((?:\s*//[^\n]*\n)*)\s*//[^\n]*[Ll]ead-in rest[^\n]*\n\s*\{"rest", 0\.00, 0\.560, false, false, "(\w+)"\},\n',exc)
print('leadins',[(l[0],l[2]) for l in leads])
exc2=re.sub(r'(inline const Note k\w+\[\] = \{\n(?:\s*//[^\n]*\n)*)\s*//[^\n]*[Ll]ead-in rest[^\n]*\n\s*\{"rest", 0\.00, 0\.560, false, false, "\w+"\},\n',r'\1',exc)
for name,_,dyn in leads:
    src,n=re.subn(r'\(excerpts::%s,'%name,'(withLeadIn(excerpts::%s, "%s"),'%(name,dyn),src); print(name,n)
exc2=exc2.replace('struct Note {\n  const char* name;\n  double hertz;\n  double seconds;\n  bool tongued;\n  bool vibrato;\n  const char* dynamic;\n};','using Note = ScoreNote;')
exc2=exc2.replace('namespace mmathen {','#include "mmathen/instruments/wind_presets.h"\n\nnamespace mmathen {',1)
io.open(E,'w',encoding='utf-8',newline='').write(exc2.replace('\n','\r\n') if CR else exc2)
io.open(T,'w',encoding='utf-8',newline='').write(src.replace('\n','\r\n') if CR else src)
io.open('_scratch/hdr_parts.txt','w',encoding='utf-8',newline='').write('\n'.join(hdr))

