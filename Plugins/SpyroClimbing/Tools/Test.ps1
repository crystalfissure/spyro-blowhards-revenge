param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_4.27')
$ErrorActionPreference = 'Stop'
$pluginRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path (Split-Path $pluginRoot -Parent) -Parent
$projectFile = Join-Path $projectRoot 'Spyro_Bunnited.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UE4Editor-Cmd.exe'
$logPath = Join-Path $projectRoot 'Saved/Logs/SpyroClimbing-Tests.log'
$reportPath = Join-Path $projectRoot 'Saved/Automation/SpyroClimbing'
if (!(Test-Path -LiteralPath $editor)) { throw "Editor not found: $editor" }
New-Item -ItemType Directory -Force -Path (Split-Path $logPath -Parent) | Out-Null
$runStarted = Get-Date
# Automation Quit exits after reporting results with the test status, avoiding headless Slate layout cleanup.
& $editor $projectFile /Engine/Maps/Entry -unattended -nop4 -nosplash -nosound -nullrhi -nowrite '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bRestoreOpenAssetTabsOnRestart=False' '-ExecCmds=Automation RunTests SpyroClimbing; Quit' "-ReportExportPath=$reportPath" "-abslog=$logPath"
if ($LASTEXITCODE -ne 0) { throw "Automation failed ($LASTEXITCODE). See $logPath" }
$reportFile = Get-Item -LiteralPath (Join-Path $reportPath 'index.json')
if ($reportFile.LastWriteTime -lt $runStarted) { throw "Test report was not refreshed. See $logPath" }
$report = Get-Content -LiteralPath $reportFile.FullName -Raw | ConvertFrom-Json
if ($report.failed -gt 0 -or $report.succeeded -lt 18) { throw "Not all eighteen tests passed. See $reportPath" }
Write-Output "All eighteen tests passed. Report: $reportPath"
