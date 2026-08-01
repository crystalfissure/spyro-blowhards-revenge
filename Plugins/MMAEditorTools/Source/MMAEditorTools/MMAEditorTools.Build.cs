using UnrealBuildTool;

public class MMAEditorTools : ModuleRules
{
    public MMAEditorTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MMAGameplay" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "Kismet", "BlueprintGraph", "Json", "JsonUtilities"
        });
    }
}
