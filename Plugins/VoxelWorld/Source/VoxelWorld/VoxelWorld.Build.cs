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
            "PhysicsCore",
            "SkillCreatorCore", "RealtimeMeshComponent",
            "Json", "JsonUtilities",
            // M-10：shader 類型實際註冊在獨立的 VoxelWorldShaders 模組（PostConfigInit，
            // 見該模組 Build.cs 開頭註解），這裡只是呼叫它公開的 dispatch 函式
            "VoxelWorldShaders"
        });
    }
}
