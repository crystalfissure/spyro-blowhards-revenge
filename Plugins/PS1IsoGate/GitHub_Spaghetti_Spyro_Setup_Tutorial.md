# PS1 Ownership Gate for spyro-blowhards-revenge

This guide is written against the GitHub copy of `crystalfissure/spyro-blowhards-revenge`.

The relevant GitHub project layout is:

```text
Spyro_Bunnited.uproject
Config/DefaultEngine.ini
Config/DefaultGame.ini
Content/_CF_Project/CF_TitleScreen.umap
Content/_CF_Project/Levels/00_Blowhard_Beach.umap
Content/SpyroContent/Global_Assets/Global_UserInterface/Widgets/PressStart_Menu.uasset
Content/SpyroContent/Global_Assets/Global_UserInterface/Widgets/Button_MenuOption.uasset
```

In the GitHub copy, `Config/DefaultEngine.ini` starts the game at:

```ini
GameDefaultMap=/Game/_CF_Project/CF_TitleScreen.CF_TitleScreen
```

That means the clean gate location is the title screen, before the player is allowed to press start.

## What the gate does

The plugin does not mount, launch, or emulate the PS1 game.

It asks the player to select a local `.iso`, `.bin`, or `.cue`. Then it checks the selected image for NTSC Spyro 1 disc markers:

```text
SYSTEM.CNF
SCUS_942.28
WAD.WAD
S0
SOURCE
PETEXA0.STR
PETEXA1.STR
PETEXA2.STR
PETEXA3.STR
PETEXA4.STR
PETEXA5.STR
```

For `.cue`, it reads the CUE sheet and verifies the referenced `.bin`.

## 1. Install the plugin

Copy the plugin folder to:

```text
spyro-blowhards-revenge/
  Plugins/
    PS1IsoGate/
      PS1IsoGate.uplugin
      Source/
```

If your folder is named `PS1ISOGate`, that is fine. The important part is that the `.uplugin` file is inside that plugin folder.

Open `Spyro_Bunnited.uproject`.

If Unreal asks whether to rebuild the plugin, choose **Yes**.

If the Blueprint nodes do not appear later, close Unreal and delete these folders from the plugin:

```text
Plugins/PS1IsoGate/Binaries
Plugins/PS1IsoGate/Intermediate
```

Then reopen the project and let Unreal rebuild.

## 2. Enable the plugin

In Unreal:

1. Open **Edit > Plugins**.
2. Search for `PS1`.
3. Enable **PS1 Ownership Gate**.
4. Restart Unreal if prompted.

## 3. Add config rules

Open:

```text
Config/DefaultGame.ini
```

Add:

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

Leave `DiscImagePath` blank for the real player-facing flow. The player will select the file from the title screen.

Leave `ExpectedSha256` blank unless you want to require one exact dump. Blank means the plugin verifies by disc contents/markers.

## 4. Duplicate the title widget

Do not edit the shared global widget directly if you can avoid it.

Duplicate:

```text
Content/SpyroContent/Global_Assets/Global_UserInterface/Widgets/PressStart_Menu.uasset
```

Put the duplicate somewhere project-specific, for example:

```text
Content/_CF_Project/UI/PressStart_MenuISO.uasset
```

If `_CF_Project/UI` does not exist, create that folder.

## 5. Make the title screen use the duplicate

Open:

```text
Content/_CF_Project/CF_TitleScreen.umap
```

Open the Level Blueprint.

Find the `Create Widget` node that currently creates:

```text
PressStart_Menu
```

Change its class to your duplicate:

```text
PressStart_MenuISO
```

Compile and save the level.

## 6. Add UI elements to PressStart_MenuISO

Open:

```text
Content/_CF_Project/UI/PressStart_MenuISO.uasset
```

In the Designer:

1. Add a button named `Button_ISO`.
2. Put a `TextBlock` inside it with visible text like `Choose ISO`.
3. Add a status `TextBlock` named `Text_ISOStatus`.
4. Find the existing normal press-start button, usually named `Button_PressStart`.

If you cannot change visible button text, select the child `TextBlock` inside the button. A plain Unreal `Button` does not have visible text by itself.

## 7. Disable Press Start on construct

In `PressStart_MenuISO`, open the Graph.

On `Event Construct`, insert this after the existing mouse cursor setup:

```text
Event Construct
  -> existing setup
  -> Set Is Enabled
       Target: Button_ISO
       In Is Enabled: true
  -> Set Is Enabled
       Target: Button_PressStart
       In Is Enabled: false
  -> Set Text
       Target: Text_ISOStatus
       Text: "Choose your Spyro PS1 disc image to unlock play."
```

The important part is that `Button_PressStart` starts disabled.

## 8. Wire the Choose ISO button

In the Designer, select `Button_ISO`.

In Details, scroll to Events and click `+` beside `OnClicked`.

In the new graph:

```text
OnClicked(Button_ISO)
  -> Choose And Verify Configured PS1 Disc Image
  -> Break PS1IsoVerificationResult
  -> Branch
       Condition: bCanPlay
```

True branch:

```text
Set Is Enabled
  Target: Button_PressStart
  In Is Enabled: true

Set Text
  Target: Text_ISOStatus
  Text: "Verified. Press Start."
```

False branch:

```text
Set Is Enabled
  Target: Button_PressStart
  In Is Enabled: false

Set Text
  Target: Text_ISOStatus
  Text: Message
```

`Message` comes from `Break PS1IsoVerificationResult`.

## 9. Optional: remember the selected file

The picker node returns:

```text
SelectedDiscImagePath
```

If verification succeeds, save that string in your own SaveGame/settings object.

On the next title-screen load:

```text
Load saved path
  -> Verify PS1 Disc Image With Configured Rules
  -> Branch on bCanPlay
```

If true, enable Press Start immediately.

Still keep the `Choose ISO` button visible so the player can pick a new file if they moved their disc image.

## 10. Test checklist

Test in this order:

1. No file selected: Press Start stays disabled.
2. Cancel the picker: status says no image was selected.
3. Select a `.txt` or wrong extension: verification fails.
4. Select a wrong `.iso` or `.bin`: verification fails because markers are missing.
5. Select a `.cue` whose `.bin` is missing: verification fails with a CUE/BIN message.
6. Select the NTSC Spyro 1 image: `bCanPlay` is true and Press Start unlocks.
7. Package a development build and test the picker outside the editor.

## 11. If the Blueprint node is missing

If searching for `Choose And Verify` only shows the old verify nodes:

1. Close Unreal.
2. Delete:

```text
Plugins/PS1IsoGate/Binaries
Plugins/PS1IsoGate/Intermediate
```

3. Reopen `Spyro_Bunnited.uproject`.
4. Let Unreal rebuild the plugin.
5. Reopen the widget graph.
6. Right-click empty graph space, disable **Context Sensitive**, and search `PS1`.

You should see:

```text
Choose PS1 Disc Image
Choose And Verify Configured PS1 Disc Image
Verify PS1 Disc Image With Configured Rules
```

## 12. Shipping notes

This is a gate, not strong DRM. It is a user-friendly ownership check.

Do not ship any PS1 game files, BIOS files, or emulator files.

For the best player experience, explain on the title screen that they need to select their own legally owned NTSC Spyro 1 disc image.
