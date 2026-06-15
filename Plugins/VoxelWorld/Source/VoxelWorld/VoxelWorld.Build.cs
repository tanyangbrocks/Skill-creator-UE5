using UnrealBuildTool;
using System.IO;

public class VoxelWorld : ModuleRules
{
    public VoxelWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // ThirdParty/FastNoiseLite.h：單一標頭，不需要額外 .lib
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "RenderCore", "RHI",
            "SkillCreatorCore", "RealtimeMeshComponent"
        });
    }
}
