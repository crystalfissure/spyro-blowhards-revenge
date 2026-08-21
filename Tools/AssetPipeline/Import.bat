@echo off
setlocal enabledelayedexpansion

rem Headless import: runs the pipeline in a commandlet with no editor UI.
rem Use this when the editor is CLOSED. If it is open, send_to_editor.py is
rem much faster and you see the assets appear live.
rem
rem   Import.bat manifests\my_batch.json
rem   Import.bat manifests\my_batch.json --dry-run

set "ENGINE=C:\Unreal Engine\UE_4.27"
set "PROJECT=%~dp0..\..\Spyro_Bunnited.uproject"

if "%~1"=="" (
    echo Usage: Import.bat ^<manifest.json^> [--dry-run]
    exit /b 1
)
if not exist "%~f1" (
    echo Manifest not found: %~f1
    exit /b 1
)

set "UE_ASSET_MANIFEST=%~f1"
set "UE_ASSET_DRY_RUN="
if /i "%~2"=="--dry-run" set "UE_ASSET_DRY_RUN=1"

tasklist /fi "imagename eq UE4Editor.exe" 2>nul | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo.
    echo   The Unreal editor is running. A headless import would fight it over
    echo   the same package files. Close the editor, or use:
    echo       python "%~dp0send_to_editor.py" "%~f1"
    echo.
    exit /b 1
)

rem UE 4.27 resolves -script= by splitting the value at the first space, so a
rem script living under "...\Spyro Fangame Engine\..." never resolves and gets
rem exec'd as a Python statement instead. It also runs the value through escape
rem processing, which eats "\u" and friends. Staging a copy under a space-free
rem path and passing it with forward slashes sidesteps both.
set "STAGE=%TEMP%\UEAssetPipeline"
if not exist "%STAGE%" mkdir "%STAGE%" >nul 2>&1
copy /y "%~dp0ue_asset_pipeline.py" "%STAGE%\ue_asset_pipeline.py" >nul
if errorlevel 1 (
    echo Could not stage the importer into "%STAGE%".
    exit /b 1
)

set "SCRIPT=%STAGE%\ue_asset_pipeline.py"
set "SCRIPT=%SCRIPT:\=/%"

echo %SCRIPT%| find " " >nul
if not errorlevel 1 (
    echo.
    echo   Your TEMP path contains a space:
    echo       %TEMP%
    echo   Unreal 4.27 cannot load a -script= path with spaces in it. Set TEMP
    echo   to a space-free folder before running this, e.g.
    echo       set TEMP=C:\Temp
    echo.
    exit /b 1
)

echo Project  : %PROJECT%
echo Manifest : %UE_ASSET_MANIFEST%
if defined UE_ASSET_DRY_RUN echo Mode     : DRY RUN
echo.

"%ENGINE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJECT%" ^
    -run=pythonscript -script="%SCRIPT%" ^
    -unattended -nopause -nosplash -stdout -FullStdOutLogOutput

set "CODE=%ERRORLEVEL%"
echo.
if "%CODE%"=="0" (
    echo Import finished.
) else (
    echo Import exited with code %CODE% - check the log above for [assetpipe] lines.
)
exit /b %CODE%
