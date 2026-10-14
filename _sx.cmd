@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _sx_build.txt 2>&1
echo build exit=%ERRORLEVEL%
if not exist _wsx mkdir _wsx
pushd _wsx
..\build_ninja_rel\mmathen_tests.exe "wind excerpts are audition passages" > ..\_sx_run.txt 2>&1
echo run exit=%ERRORLEVEL%
popd
