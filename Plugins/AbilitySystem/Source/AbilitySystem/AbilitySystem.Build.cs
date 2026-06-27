using UnrealBuildTool;

public class AbilitySystem : ModuleRules
{
    public AbilitySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "StructUtils",
            "GameplayAbilities", "GameplayTags", "GameplayTasks",
            "SkillCreatorCore"
        });
    }
}
