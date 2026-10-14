@call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /O2 /EHsc /I..\include z.cpp && z.exe 1 > z1.txt && z.exe 0 > z0.txt
