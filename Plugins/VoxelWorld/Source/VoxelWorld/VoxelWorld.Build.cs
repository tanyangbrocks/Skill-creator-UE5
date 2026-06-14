using UnrealBuildTool;

public class VoxelWorld : ModuleRules
{
    public VoxelWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "RenderCore", "RHI",
            "SkillCreatorCore"
            // RealtimeMeshComponent 裝好後加入此處
        });
    }
}
