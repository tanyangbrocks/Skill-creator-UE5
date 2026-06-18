using UnrealBuildTool;

public class SkillCreatorUI : ModuleRules
{
    public SkillCreatorUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine",
            "Slate", "SlateCore", "GraphEditor", "EditorStyle",
            "StructUtils", "ToolMenus",
            "SkillCreatorCore", "AbilitySystem"
        });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "UnrealEd"
        });
    }
}
