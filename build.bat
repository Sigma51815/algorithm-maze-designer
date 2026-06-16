@echo off
:: Maze Designer — Windows native build script
:: Prerequisites: CMake 3.16+, Qt 6.x (with Widgets), Visual Studio 2019+
::
:: Usage:
::   build.bat              Build Release
::   build.bat debug        Build Debug
::   build.bat test         Build Release + run self-test
::   build.bat run          Build Release + launch GUI
::   build.bat clean        Remove build directory

setlocal enabledelayedexpansion
set BUILD_DIR=build
set BUILD_TYPE=Release
set TARGET=%BUILD_DIR%\Release\maze_designer.exe
set ACTION=build

if /I "%1"=="debug" (
    set BUILD_TYPE=Debug
    set TARGET=%BUILD_DIR%\Debug\maze_designer.exe
) else if /I "%1"=="test" (
    set ACTION=test
) else if /I "%1"=="run" (
    set ACTION=run
) else if /I "%1"=="clean" (
    set ACTION=clean
) else if /I "%1"=="help" (
    goto :help
) else if not "%1"=="" (
    echo Unknown option: %1
    echo Run 'build.bat help' for usage.
    exit /b 1
)

if "%ACTION%"=="clean" (
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
        echo Build directory removed.
    ) else (
        echo No build directory to clean.
    )
    exit /b 0
)

:: Configure
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Configuring CMake (%BUILD_TYPE%)...
    cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    if errorlevel 1 (
        echo CMake configuration failed.
        exit /b 1
    )
)

:: Build
echo Building...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

:: Run
if "%ACTION%"=="test" (
    echo Running self-test...
    "%TARGET%" --self-test
) else if "%ACTION%"=="run" (
    echo Launching GUI...
    start "" "%TARGET%"
) else (
    echo Build succeeded. Binary: %TARGET%
)

exit /b 0

:help
echo Maze Designer — Windows build script
echo.
echo   build.bat              Build Release (default)
echo   build.bat debug        Build Debug
echo   build.bat test         Build Release + run self-test
echo   build.bat run          Build Release + launch GUI
echo   build.bat clean        Remove build directory
echo   build.bat help         Show this help
echo.
echo Prerequisites: CMake 3.16+, Qt 6.x, Visual Studio 2019+
exit /b 0
