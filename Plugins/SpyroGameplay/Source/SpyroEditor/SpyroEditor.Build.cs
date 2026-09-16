using UnrealBuildTool;

public class SpyroEditor : ModuleRules
{
    public SpyroEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "SpyroGameplay" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "Kismet", "BlueprintGraph"
        });
    }
}
