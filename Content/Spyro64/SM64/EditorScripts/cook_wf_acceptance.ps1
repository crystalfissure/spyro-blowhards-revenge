param(
    [string]$Project = 'C:\Users\adace\Desktop\spyro-blowhards-revenge\Spyro_Bunnited.uproject',
    [string]$EditorCmd = 'C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor-Cmd.exe'
)

$maps = @(
    '/Game/Spyro64/64_TitleScreen',
    '/Game/Spyro64/Levels/00_Homeworld',
    '/Game/Spyro64/Levels/05_Level5'
) -join '+'

& $EditorCmd $Project -run=Cook -TargetPlatform=WindowsNoEditor "-Map=$maps" `
    -iterate -unversioned -unattended -nop4 -nosplash -nullrhi -log
exit $LASTEXITCODE
