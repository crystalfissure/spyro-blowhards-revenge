# PS1 Ownership Gate for Spyro_Bunnited

The plugin's rules are built in. Select the required Spyro game in your Blueprint; no DefaultGame.ini entries are needed.

## Title screen integration

1. Enable **PS1 Ownership Gate**, rebuild with Unreal 4.27, and restart the editor.
2. Duplicate Content/SpyroContent/Global_Assets/Global_UserInterface/Widgets/PressStart_Menu.uasset into your project UI folder if you want a separate menu widget.
3. In Content/_CF_Project/CF_TitleScreen.umap, change the Create Widget class to that duplicate.
4. Add a Choose ISO button and a status text block to the widget.
5. On Event Construct, disable the normal Press Start button.
6. On Choose ISO clicked, call **Choose And Verify Configured PS1 Disc Image** and select **Game**. For a game requiring Spyro 1, choose **Spyro 1**; any of the three supplied Spyro 1 regions is accepted.
7. Break **PS1 Iso Verification Result** and branch on **bCanPlay**. Enable Press Start on success. On failure, keep it disabled and display **Message**.
8. Save **Selected Disc Image Path** in your SaveGame only after successful verification if you want to remember the choice.
9. On the next launch, call **Verify PS1 Disc Image** with that saved path and the same **Game**.
10. Test the menu in the editor and in a packaged build.

## Updating an existing gate

Refresh or recreate verification nodes after rebuilding so their **Game** pins appear. **Verify PS1 Disc Image** has only **Game** and **Disc Image Path** inputs.

Replace the removed **Verify Configured PS1 Disc Image** and **Verify PS1 Disc Image With Configured Rules** nodes with **Verify PS1 Disc Image**. Refresh result-break nodes after removal of the hash fields. Any old PS1IsoGateSettings section in Config/DefaultGame.ini is unused.

The gate checks the executable first, then SYSTEM.CNF and the matched region's files and folders. It reads directory metadata without scanning or hashing the whole image. See README.md for the supported regional executables and image layouts.
