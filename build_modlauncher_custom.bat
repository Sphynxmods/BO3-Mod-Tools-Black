@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%.") do set SRC_DIR=%%~fI
for %%I in ("%SRC_DIR%\..\..") do set GAME_ROOT=%%~fI

if "%QTDIR%"=="" (
    if exist "C:\Qt\6.7.0\msvc2022_64\bin\moc.exe" (
        set QTDIR=C:\Qt\6.7.0\msvc2022_64
    ) else if exist "C:\Qt\6.7.0\msvc2019_64\bin\moc.exe" (
        set QTDIR=C:\Qt\6.7.0\msvc2019_64
    ) else if exist "C:\Qt\6.5.3\msvc2019_64\bin\moc.exe" (
        set QTDIR=C:\Qt\6.5.3\msvc2019_64
    ) else (
        set QTDIR=C:\Qt\6.5.3\msvc2019_64
    )
)
if "%TL_SDK%"=="" (
    if exist "C:\SteamworksSDK\Steamworks\steamworks_sdk_164\sdk\public\steam\steam_api.h" (
        set TL_SDK=C:\SteamworksSDK\Steamworks\steamworks_sdk_164\sdk
    ) else (
        set TL_SDK=C:\SteamworksSDK
    )
)
set LOCAL_STEAM_HEADER=%SRC_DIR%\sdk\public\steam\steam_api.h
set LOCAL_STEAM_LIB=%SRC_DIR%\sdk\redistributable_bin\win64\steam_api64.lib
set LOCAL_STEAM_DLL=%SRC_DIR%\sdk\redistributable_bin\win64\steam_api64.dll
set STEAM_HEADER=%TL_SDK%\Steamworks\sdk-1.37\public\steam\steam_api.h
set STEAM_LIB=%TL_SDK%\Steamworks\sdk-1.37\redistributable_bin\win64\steam_api64.lib
set STEAM_DLL=%TL_SDK%\Steamworks\sdk-1.37\redistributable_bin\win64\steam_api64.dll

if exist "%TL_SDK%\public\steam\steam_api.h" if exist "%TL_SDK%\redistributable_bin\win64\steam_api64.lib" (
    set STEAM_HEADER=%TL_SDK%\public\steam\steam_api.h
    set STEAM_LIB=%TL_SDK%\redistributable_bin\win64\steam_api64.lib
    set STEAM_DLL=%TL_SDK%\redistributable_bin\win64\steam_api64.dll
)

if /I "%USE_LOCAL_STEAM_SDK%"=="1" if exist "%LOCAL_STEAM_HEADER%" if exist "%LOCAL_STEAM_LIB%" (
    set STEAM_HEADER=%LOCAL_STEAM_HEADER%
    set STEAM_LIB=%LOCAL_STEAM_LIB%
    set STEAM_DLL=%LOCAL_STEAM_DLL%
)

set TA_CODE_PATH=%SRC_DIR%
set TA_TOOLS_PATH=%GAME_ROOT%
set TA_EXECUTABLE_SUFFIX=_custom
set PLATFORM=x64
set CONFIG=External_Release

if not exist "%QTDIR%\bin\moc.exe" (
    echo ERROR: QTDIR is not set to an MSVC Qt kit.
    echo Expected: %QTDIR%\bin\moc.exe
    echo Checked common locations under C:\Qt\6.7.0 and C:\Qt\6.5.3.
    echo Install an MSVC Qt kit and set QTDIR before running this script.
    exit /b 1
)

if not exist "%STEAM_HEADER%" (
    echo ERROR: Steamworks SDK header not found.
    echo Expected one of:
    echo   %TL_SDK%\Steamworks\sdk-1.37\public\steam\steam_api.h
    echo   %SRC_DIR%\sdk\public\steam\steam_api.h
    echo Set TL_SDK to your Steamworks SDK root, or copy the SDK files into the local sdk folder.
    exit /b 1
)

if not exist "%STEAM_LIB%" (
    echo ERROR: Steamworks import library not found.
    echo Expected one of:
    echo   %TL_SDK%\Steamworks\sdk-1.37\redistributable_bin\win64\steam_api64.lib
    echo   %SRC_DIR%\sdk\redistributable_bin\win64\steam_api64.lib
    exit /b 1
)

if not exist "%STEAM_DLL%" (
    echo ERROR: Steamworks redistributable DLL not found.
    echo Expected:
    echo   %STEAM_DLL%
    exit /b 1
)

set VSCMD_BAT=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat
if not exist "%VSCMD_BAT%" set VSCMD_BAT=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat
if not exist "%VSCMD_BAT%" set VSCMD_BAT=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\LaunchDevCmd.bat

if not exist "%VSCMD_BAT%" (
    echo ERROR: Visual Studio developer command script not found.
    exit /b 1
)

set MSBUILD_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
if not exist "%MSBUILD_EXE%" set MSBUILD_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe

if not exist "%MSBUILD_EXE%" (
    echo ERROR: MSBuild.exe not found.
    exit /b 1
)

call "%VSCMD_BAT%" -host_arch=x64 -arch=x64
if errorlevel 1 exit /b %errorlevel%

pushd "%SRC_DIR%"
"%MSBUILD_EXE%" ModLauncher.sln /m /p:Configuration=%CONFIG% /p:Platform=%PLATFORM%
set BUILD_EXIT=%ERRORLEVEL%
popd

if not "%BUILD_EXIT%"=="0" exit /b %BUILD_EXIT%

echo.
echo Build complete.
echo EXE: %GAME_ROOT%\bin\ModLauncher_custom.exe
echo Steam DLL: %STEAM_DLL%
echo.
echo To run it:
echo   1. Run modtools\rebuild_modlauncher_custom.bat to sync the runtime folder.
echo   2. Launch modtools\launch_modtools_variant.bat

endlocal
