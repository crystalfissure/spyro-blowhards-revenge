param(
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_4.27',
    [ValidateSet('2019', '2022')][string]$Compiler = '2022'
)
$ErrorActionPreference = 'Stop'
$pluginRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path (Split-Path $pluginRoot -Parent) -Parent
$projectFile = Join-Path $projectRoot 'Spyro_Bunnited.uproject'
$buildTool = Join-Path $EngineRoot 'Engine/Binaries/DotNET/UnrealBuildTool.exe'
$logPath = Join-Path $projectRoot 'Saved/Logs/SpyroClimbing-Build.log'
if (!(Test-Path -LiteralPath $projectFile)) { throw "Project not found: $projectFile" }
if (!(Test-Path -LiteralPath $buildTool)) { throw "UnrealBuildTool not found: $buildTool" }
New-Item -ItemType Directory -Force -Path (Split-Path $logPath -Parent) | Out-Null
& $buildTool UE4Editor Win64 Development "-Project=$projectFile" "-Plugin=$pluginRoot/SpyroClimbing.uplugin" "-$Compiler" -NoHotReloadFromIDE -NoUBTMakefiles "-Log=$logPath"
if ($LASTEXITCODE -ne 0) { throw "Plugin build failed ($LASTEXITCODE). See $logPath" }
Write-Output "Plugin built. Log: $logPath"
