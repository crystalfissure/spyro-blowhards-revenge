# PS1 Ownership Gate: Unreal setup and testing

## Setup

Enable **PS1 Ownership Gate** in Unreal 4.27 and rebuild the plugin. No PS1IsoGate configuration is needed.

On your menu's Choose File button, call **Choose And Verify Configured PS1 Disc Image** and set **Game** to **Spyro 1**, **Spyro 2**, or **Spyro 3**. Break the returned result and enable Play only if **bCanPlay** is true. Show **Message** on failure.

To recheck a saved selection without opening the picker, call **Verify PS1 Disc Image** with **Game** and **Disc Image Path**. Use the same game selection that your menu requires.

## Migration

After rebuilding, restart Unreal and refresh or recreate old nodes. Both verification nodes now take **Game**. **Verify PS1 Disc Image** takes only **Game** and **Disc Image Path**.

Replace **Verify Configured PS1 Disc Image** and **Verify PS1 Disc Image With Configured Rules** with **Verify PS1 Disc Image**. Refresh **Break PS1 Iso Verification Result** nodes to remove the deleted hash outputs. Remove obsolete PS1IsoGateSettings entries from DefaultGame.ini if present.

## Automated checks

In Unreal's Session Frontend, run the tests under **PS1IsoGate**:

- **Regions**: all eight supplied regional variants, wrong selected games, and removal of each required file or folder.
- **Formats**: all regions with cooked ISO, raw Mode 1/2352, Mode 2/2352, and Mode 2/2336 data; quoted CUE paths, stored pregaps, and selection of the data track after an audio file.
- **Failures**: missing paths, unsupported game/extension, mismatched boot target, forged filename bytes, malformed or oversized root directories, truncated extents, and invalid CUE sheets.

Synthetic fixtures are created under Saved/PS1IsoGateTests and cleaned up after each test.

## Manual checks

Use your own images to verify Spyro 1 and 2 NTSC-U/PAL/NTSC-J and Spyro 3 NTSC-U/PAL. Confirm that selecting another game fails immediately at the executable check. Try both direct images and their CUE sheets.

Cancel the Windows picker, choose an unsupported extension, and select a CUE whose binary data file is missing. Play must stay disabled, and Message must explain the failure. Test the picker in a packaged build as well as the editor.
