@echo off
setlocal
pushd "%~dp0"

set "QTFRAMEWORK_BYPASS_LICENSE_CHECK=1"
set "QT_ROOT=F:\Qt\6.11.1\mingw_64"
set "CMAKE=F:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA=F:\Qt\Tools\Ninja\ninja.exe"
set "CXX=F:\Qt\Tools\mingw1310_64\bin\g++.exe"

"%CMAKE%" -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DCMAKE_CXX_COMPILER="%CXX%" -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 goto :fail
"%CMAKE%" --build build --parallel
if errorlevel 1 goto :fail

set "PATH=%QT_ROOT%\bin;F:\Qt\Tools\mingw1310_64\bin;%PATH%"
start "" "build\maze_designer.exe"
popd
exit /b 0

:fail
echo Build failed. Please check the Qt installation paths in this file.
pause
popd
exit /b 1
