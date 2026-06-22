using UnrealBuildTool;

// M-10：獨立、輕量的 shader-only 模組，跟主要的 VoxelWorld（TileWorld3D/Chunk3D 等遊戲邏輯）
// 分開，原因見 docs/plan-m10-gpu-ca.md「Phase 0 問題回報」：IMPLEMENT_GLOBAL_SHADER 在模組
// DLL 載入當下做靜態註冊，VoxelWorld 主模組是 LoadingPhase=Default，這時候引擎的 shader type
// 註冊表已經鎖住，新 shader type 會直接讓引擎 assert 退出
// （Shader.h:1594 AreShaderTypesInitialized）。
//
// 這個模組只依賴 Core/RenderCore/RHI/Projects（本來就載入得很早的引擎模組），
// 設成 LoadingPhase=PostConfigInit，才能在 shader 註冊表鎖住之前完成載入。
// 不依賴 SkillCreatorCore/RealtimeMeshComponent/Engine——這些都是 Default 階段才載入，
// 若這個模組依賴它們，會比依賴對象更早載入而失敗。
public class VoxelWorldShaders : ModuleRules
{
    public VoxelWorldShaders(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "RenderCore", "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Projects"
        });
    }
}
