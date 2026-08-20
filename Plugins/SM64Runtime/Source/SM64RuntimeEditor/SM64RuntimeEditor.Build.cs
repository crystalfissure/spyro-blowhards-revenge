using UnrealBuildTool;

public class SM64RuntimeEditor : ModuleRules
{
    public SM64RuntimeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetTools",
            "BlueprintGraph",
            "UnrealEd",
            "Kismet"
        });
    }
}
