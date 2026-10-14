@call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _scratch\bl.txt 2>&1 || (type _scratch\bl.txt & exit /b 1)
cd _scratch && ..\build_ninja_rel\mmathen_tests.exe "wind excerpts are audition passages" > ph.txt 2>&1
echo exit=%ERRORLEVEL%
