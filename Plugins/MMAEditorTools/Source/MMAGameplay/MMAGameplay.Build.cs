using UnrealBuildTool;

public class MMAGameplay : ModuleRules
{
    public MMAGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "AIModule"
        });
    }
}
