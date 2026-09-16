using UnrealBuildTool;

public class SpyroGameplay : ModuleRules
{
    public SpyroGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine"
        });
    }
}
