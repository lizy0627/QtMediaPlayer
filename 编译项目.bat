@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

echo ========================================
echo QtMediaPlayer - out-of-source build
echo ========================================
echo.

set "QT_DIR=D:\QT\6.5.3\mingw_64"
set "MINGW_DIR=D:\QT\Tools\mingw1120_64"
set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%build\qmake"

if exist "%QT_DIR%\bin" (
    set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"
)

echo [1/5] Checking Qt tools...
where qmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: qmake was not found. Check QT_DIR or PATH.
    exit /b 1
)

where mingw32-make >nul 2>&1
if errorlevel 1 (
    echo ERROR: mingw32-make was not found. Check MINGW_DIR or PATH.
    exit /b 1
)

for /f %%v in ('qmake -query QT_VERSION') do set "QT_VERSION=%%v"
if "%QT_VERSION%"=="" (
    echo ERROR: Could not read Qt version from qmake.
    exit /b 1
)

echo Found Qt %QT_VERSION%
echo %QT_VERSION% | findstr /r "^6\." >nul
if errorlevel 1 (
    echo ERROR: Qt 6.x is required for this project.
    exit /b 1
)

echo.
echo [2/5] Preparing build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if errorlevel 1 (
    echo ERROR: Could not create build directory: %BUILD_DIR%
    exit /b 1
)

echo.
echo [3/5] Generating Makefiles in build\qmake...
pushd "%BUILD_DIR%"
qmake "%PROJECT_DIR%QtMediaPlayer.pro"
if errorlevel 1 (
    popd
    echo ERROR: qmake failed.
    exit /b 1
)

echo.
echo [4/5] Building project...
mingw32-make -j4
if errorlevel 1 (
    popd
    echo ERROR: build failed.
    exit /b 1
)

echo.
echo [5/5] Build finished.
echo Output directory:
echo   %BUILD_DIR%\bin
popd

echo.
echo Done.
endlocal
