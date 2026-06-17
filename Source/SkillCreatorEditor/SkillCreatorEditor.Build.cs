using UnrealBuildTool;

public class SkillCreatorEditor : ModuleRules
{
    public SkillCreatorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd",
            "Slate", "SlateCore", "GraphEditor",
            "SkillCreatorCore", "SkillCreatorRuntime", "SkillCreatorUI"
        });
    }
}
