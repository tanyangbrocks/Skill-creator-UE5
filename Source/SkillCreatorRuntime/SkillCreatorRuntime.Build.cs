using UnrealBuildTool;

public class SkillCreatorRuntime : ModuleRules
{
    public SkillCreatorRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "GameplayTags",
            "SkillCreatorCore"
        });
    }
}
