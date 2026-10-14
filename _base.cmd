@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _base_build.txt 2>&1
echo build exit=%ERRORLEVEL%
if exist _wav rmdir /s /q _wav
mkdir _wav
pushd _wav
..\build_ninja_rel\mmathen_tests.exe "[wind],[.songs]" > ..\_base_run.txt 2>&1
echo run exit=%ERRORLEVEL%
popd
if exist _wav_base rmdir /s /q _wav_base
move _wav _wav_base
