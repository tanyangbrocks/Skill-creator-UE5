using UnrealBuildTool;

public class SkillCreatorCore : ModuleRules
{
    public SkillCreatorCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "StructUtils", "GameplayTags"
        });
    }
}
