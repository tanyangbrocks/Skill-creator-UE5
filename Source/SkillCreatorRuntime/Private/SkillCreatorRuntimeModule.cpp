#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ItemRegistry.h"
#include "ItemId.h"
#include "APlacedFixtureActor.h"

// 2026-06-23：物品系統擴充——FItemData.PlaceAsActor 是泛型 TSubclassOf<AActor>（避免
// SkillCreatorCore 反過來依賴 SkillCreatorRuntime），實際的 AChestActor/AWorkbenchActor
// 類別要在 Runtime 模組啟動時「後置註冊」進 FItemRegistry。StartupModule() 是這個模組
// 的程式碼保證已經載入、但還沒有任何 World/GameMode 存在的最早時間點，比塞進
// ASkillCreatorGameMode::BeginPlay() 更乾淨（不需要每次 BeginPlay 都重新設定一次）。
class FSkillCreatorRuntimeModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override;
};

void FSkillCreatorRuntimeModule::StartupModule()
{
    FItemRegistryPlaceableActors::RegisterAll();
}

IMPLEMENT_MODULE(FSkillCreatorRuntimeModule, SkillCreatorRuntime)
