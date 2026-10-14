#include <cstdio>
#include "mmathen/instruments/player_airway.h"
using namespace mmathen;
int main(int argc,char**argv){ PlayerAirwaySpec s; if(argc>1) s.tractLossScale=atof(argv[1]); if(argc>2) s.glottisAreaM2=atof(argv[2]);
 PlayerAirway a(s);
 double lo=argc>3?atof(argv[3]):100,hi=argc>4?atof(argv[4]):1500,st=argc>5?atof(argv[5]):20; for(double f=lo;f<=hi;f+=st) printf("%g %.3g\n",f,PlayerAirway::inputImpedanceMagnitude(a,f,0.6)); }
