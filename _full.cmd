@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _full_build.txt 2>&1
echo build exit=%ERRORLEVEL%
if not exist _wfull mkdir _wfull
pushd _wfull
..\build_ninja_rel\mmathen_tests.exe > ..\_full_run.txt 2>&1
echo run exit=%ERRORLEVEL%
popd
