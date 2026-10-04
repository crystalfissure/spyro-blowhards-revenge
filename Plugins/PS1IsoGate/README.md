# PS1 Ownership Gate

Unreal Engine 4.27 runtime plugin that verifies a player-selected Spyro PS1 disc image before your game enables play. All verification rules are compiled into the plugin; no configuration file is needed.

## Supported games and regions

The Blueprint enum **PS1 Iso Game** (C++: `EPS1IsoGame`) has exactly three values: **Spyro 1**, **Spyro 2**, and **Spyro 3**.

| Game | NTSC-U executable | PAL executable | NTSC-J executable |
| --- | --- | --- | --- |
| Spyro 1 | SCUS_942.28 | SCES_014.38 | SCPS_100.85 |
| Spyro 2 | SCUS_944.25 | SCES_021.04 | SCPS_101.28 |
| Spyro 3 | SCUS_944.67 | SCES_028.35 | Not listed in the supplied reference |

The complete regional file and folder lists are hard-coded from `Info/PS1IsoGate/PS1IsoGate Info.txt`. The executable selects the regional list. Japanese Spyro 1 does not require S0, and Japanese Spyro 2 does not require KART. Spyro 3 NTSC-U requires 3MN_BLNK.DAT; PAL requires SPYRO3.TRD.

## Blueprint nodes

- **Verify PS1 Disc Image**: inputs **Game** and **Disc Image Path**; returns **PS1 Iso Verification Result**.
- **Choose And Verify Configured PS1 Disc Image**: input **Game**; opens the Windows picker and returns **Selected Disc Image Path** and the verification result. Its existing name is retained, but its rules are built in.
- **Choose PS1 Disc Image**: opens the picker and returns the selected path without verification.

Use **bCanPlay** to enable play. **Message** explains failures. **BootExecutable** reports the executable read from SYSTEM.CNF, and **MissingFiles** reports missing or invalid required entries.

## Verification and speed

Accepts ISO, BIN, and CUE extensions, ignoring case. It reads ISO 9660 directory metadata, checks the selected game's executable alternatives first, then reads SYSTEM.CNF to confirm the boot target and checks that region's remaining entries. Files and folders must have the correct directory entry type and an extent within the image.

The reader supports cooked 2048-byte sectors, raw Mode 1 and Mode 2/XA 2352-byte sectors, and Mode 2 2336-byte sectors. For CUE, it resolves the first supported binary data track and honors INDEX 01, including pregaps stored in the file.

Only directory metadata and the small SYSTEM.CNF file are read. No whole-image scan, file hash calculation, or game asset loading is performed. Root metadata and SYSTEM.CNF reads are bounded.

## Install and use

1. Copy PS1IsoGate into your project's Plugins folder, enable **PS1 Ownership Gate**, and rebuild with Unreal 4.27.
2. Add **Choose And Verify Configured PS1 Disc Image** to your menu and select the required **Game**.
3. Enable your Play button only when **bCanPlay** is true; otherwise show **Message**.
4. Save **Selected Disc Image Path** in your own SaveGame if desired.
5. On the next launch, call **Verify PS1 Disc Image** with the same **Game** and the saved path.

## Updating older Blueprints

Restart Unreal after rebuilding. Refresh or recreate existing verification nodes to expose **Game** and remove obsolete pins. Refresh result-break nodes after removing the old hash fields.

**Verify Configured PS1 Disc Image**, **Verify PS1 Disc Image With Configured Rules**, the PS1IsoGateSettings class, and ConfigExample.ini have been removed. Replace those old verification nodes with **Verify PS1 Disc Image** and pass the path explicitly. Old PS1IsoGate settings in DefaultGame.ini are unused and can be removed.

## Validation

Development/editor builds include **PS1IsoGate.Regions**, **PS1IsoGate.Formats**, and **PS1IsoGate.Failures** automation tests. The fixtures contain synthetic directory records and text, with no game data. Tests cover the eight regional lists, wrong-game rejection, each required entry, sector layouts, CUE handling, boot-target mismatch, malformed metadata, and truncated images.

This plugin verifies disc structure. It does not mount or emulate the disc or supply game files.
