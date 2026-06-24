using UnrealBuildTool;

public class SkillCreatorRuntime : ModuleRules
{
    public SkillCreatorRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "GameplayTags",
            "EnhancedInput", "AIModule", "NavigationSystem", "GameplayTasks",
            "SkillCreatorCore", "AbilitySystem", "VoxelWorld", "NPCBrain",
            "Json", "JsonUtilities",
            "UMG", "SlateCore", "Slate"
        });

        // 積木編輯器 overlay（僅 Editor Build 使用）
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "SkillCreatorUI", "GraphEditor" });
        }
    }
}
