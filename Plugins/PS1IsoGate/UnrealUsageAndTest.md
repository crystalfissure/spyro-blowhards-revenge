# PS1 Ownership Gate: Unreal Setup and Test

## 1. Install the plugin

Copy the `PS1IsoGate` folder into your Unreal project:

```text
YourProject/
  Plugins/
    PS1IsoGate/
      PS1IsoGate.uplugin
```

Restart Unreal, then enable `PS1 Ownership Gate` in the Plugins window.

If your project uses C++, regenerate project files and compile. If it is Blueprint-only, Unreal may ask to rebuild the plugin on startup.

## 2. Configure the disc image

Add this to your project's `Config/DefaultGame.ini`:

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

Use `.cue`, `.bin`, or `.iso`. For `.cue`, the plugin reads the referenced `.bin` file.

`DiscImagePath` can stay blank for the real player flow. The player will pick the file from your menu.

## 3. Blueprint menu flow

In your main menu widget:

1. Add a `Choose File` button.
2. On click, call `Choose And Verify Configured PS1 Disc Image`.
3. Store the returned `SelectedDiscImagePath` in your own SaveGame/settings if verification succeeds.
4. Break the returned `PS1 Iso Verification Result`.
5. If `bCanPlay` is true:
   - Enable your Play button.
   - Show a short verified/access granted message if desired.
6. If `bCanPlay` is false:
   - Disable your Play button.
   - Show `Message` to explain what failed.

On the Play button click, open your game's first level as normal. Do not launch an emulator.

## 4. Quick test cases

Test these before shipping:

- Empty `DiscImagePath`: should fail with a missing path message.
- Wrong path: should fail with a file-not-found message.
- `.cue` pointing at a missing `.bin`: should fail with a CUE/BIN message.
- Wrong game image: should fail because the expected markers are missing.
- Correct NTSC Spyro image: should return `bCanPlay = true`.

## 5. Optional exact dump lock

Leave `ExpectedSha256` blank to accept any image that matches the expected disc markers.

Set `ExpectedSha256` only if you want to require one exact dump:

```powershell
Get-FileHash "C:\Games\PS1\SpyroTheDragon.bin" -Algorithm SHA256
```

Paste the hash into `ExpectedSha256`.
