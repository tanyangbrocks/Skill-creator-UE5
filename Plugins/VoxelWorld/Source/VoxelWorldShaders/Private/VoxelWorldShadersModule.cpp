#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

// M-10：註冊 VoxelWorld plugin 的 Shaders/ 目錄，讓 .usf 檔可以用
// "/Plugin/VoxelWorld/xxx.usf" 虛擬路徑在 IMPLEMENT_GLOBAL_SHADER 裡引用。
// 注意：shader 目錄掛在 VoxelWorld plugin 本身（FindPlugin("VoxelWorld")），不是這個
// VoxelWorldShaders 模組——UE5 的 shader 目錄對應是 plugin 層級，跟 .usf 檔實際放在
// Plugins/VoxelWorld/Shaders/ 對應，哪個 C++ 模組負責註冊不影響這個路徑。
class FVoxelWorldShadersModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        const FString PluginShaderDir = FPaths::Combine(
            IPluginManager::Get().FindPlugin(TEXT("VoxelWorld"))->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/VoxelWorld"), PluginShaderDir);
    }
};

IMPLEMENT_MODULE(FVoxelWorldShadersModule, VoxelWorldShaders)
