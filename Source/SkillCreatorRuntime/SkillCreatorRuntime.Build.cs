using UnrealBuildTool;

public class SkillCreatorRuntime : ModuleRules
{
    public SkillCreatorRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "GameplayTags",
            "EnhancedInput", "AIModule", "NavigationSystem", "GameplayTasks",
            "SkillCreatorCore", "AbilitySystem", "VoxelWorld",
            "Json", "JsonUtilities",
            "UMG", "SlateCore", "Slate"
        });
    }
}
