# PS1 Ownership Gate

Drop-in Unreal Engine 4.27 runtime plugin for checking that the player has a local, legally owned NTSC PS1 Spyro disc image before enabling access to your Unreal game.

## What it does

- Checks that the configured disc image exists.
- Accepts configured extensions: `iso`, `bin`, and `cue` by default.
- For `.cue` files, resolves the referenced `.bin` file and checks that data.
- Scans the disc image for the expected NTSC Spyro markers, including `SYSTEM.CNF`, boot executable `SCUS_942.28`, and the expected disc files.
- Streams the image in chunks, so normal marker verification does not load the whole PS1 image into memory.
- Optionally streams SHA-256 and compares it to the expected hash when `ExpectedSha256` is set.
- Exposes Blueprint-callable functions:
  - `Verify Configured PS1 Disc Image`
  - `Verify PS1 Disc Image`
  - `Verify PS1 Disc Image With Configured Rules`
  - `Choose PS1 Disc Image`
  - `Choose And Verify Configured PS1 Disc Image`

## Install

1. Copy the `PS1IsoGate` folder into your Unreal project's `Plugins` folder.
2. Restart Unreal.
3. Enable `PS1 Ownership Gate` in the Plugins window.
4. Add settings to your project's `Config/DefaultGame.ini` using `ConfigExample.ini` as a starting point.

## Configure

Add this to `Config/DefaultGame.ini`.

`DiscImagePath` is only needed if you want a fixed default path for testing. For the player-facing flow, let the user choose a file in your menu and leave `DiscImagePath` blank.

```ini
[/Script/PS1IsoGate.PS1IsoGateSettings]
DiscImagePath=
ExpectedSha256=
+AllowedExtensions=iso
+AllowedExtensions=bin
+AllowedExtensions=cue
ExpectedBootExecutable=SCUS_942.28
+RequiredDiscFiles=SYSTEM.CNF
+RequiredDiscFiles=SCUS_942.28
+RequiredDiscFiles=WAD.WAD
+RequiredDiscFiles=S0
+RequiredDiscFiles=SOURCE
+RequiredDiscFiles=PETEXA0.STR
+RequiredDiscFiles=PETEXA1.STR
+RequiredDiscFiles=PETEXA2.STR
+RequiredDiscFiles=PETEXA3.STR
+RequiredDiscFiles=PETEXA4.STR
+RequiredDiscFiles=PETEXA5.STR
```

`ExpectedSha256` can stay blank if you only want to verify the disc markers. If you want to require one exact dump, get the SHA-256 on Windows:

```powershell
Get-FileHash "C:\Games\PS1\SpyroTheDragon.bin" -Algorithm SHA256
```

## Blueprint flow

Recommended player-facing flow:

- Add a `Choose File` button to your menu.
- On click, call `Choose And Verify Configured PS1 Disc Image`.
- Save the returned `SelectedDiscImagePath` in your own SaveGame/settings if verification succeeds.
- If `bCanPlay` is true, enable access to your Unreal game.
- If false, show `Message` and keep access disabled.

If you saved a player-selected path, call `Verify PS1 Disc Image With Configured Rules` on the next launch to re-check that saved file without opening the picker again.

For a fixed test path, set `DiscImagePath` and call `Verify Configured PS1 Disc Image`.

The image check expects these NTSC disc markers inside the `.iso`, `.bin`, or `.cue` target:

- `SYSTEM.CNF`
- `SCUS_942.28`
- `WAD.WAD`
- `S0`
- `SOURCE`
- `PETEXA0.STR` through `PETEXA5.STR`

The plugin does not provide game files, BIOS files, or an emulator. It does not launch, mount, or emulate the PS1 game. It only verifies that the configured disc image is present so your Unreal game can decide whether to unlock play.
