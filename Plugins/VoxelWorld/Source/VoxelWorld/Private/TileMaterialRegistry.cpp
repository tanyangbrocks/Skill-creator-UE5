#include "TileMaterialRegistry.h"

UMaterialInterface* UTileMaterialRegistry::GetSurface(EMaterialType Type) const
{
	const int32 Idx = static_cast<int32>(Type);
	if (!Entries.IsValidIndex(Idx)) return nullptr;
	// 2026-06-23 根因修復：TSoftObjectPtr::Get() 只在「已經被別處載入」時才回傳非 null，
	// 本身完全不會觸發載入。專案裡從未實作 plan-asset-registry.md §6 規劃的非同步預載
	// （RequestAsyncLoad/PreloadCoreAssets 全專案零呼叫處），所以這個 soft ref 實際上永遠
	// 沒被載入過。Editor/PIE 測試時常因為 Content Browser 縮圖快取等原因讓材質已經常駐
	// 記憶體，造成「看起來正常」的假象；全新啟動的 Standalone 封裝版沒有這個僥倖，
	// GetSurface() 每次都拿到 nullptr → 全部 fallback 到 VoxelMaterial → 全部 fallback
	// 到 RealtimeMeshComponent 的「材質為 null」預設外觀（看起來就是整片格子狀）。
	// 改用 LoadSynchronous() 強制立即載入（只在 BeginPlay 建材質 slot 時呼叫一次，
	// 16 個小型純色材質，同步載入的一次性延遲可忽略）。
	return Entries[Idx].SurfaceMaterial.LoadSynchronous();
}

UMaterialInterface* UTileMaterialRegistry::GetEmissive(EMaterialType Type) const
{
	const int32 Idx = static_cast<int32>(Type);
	if (!Entries.IsValidIndex(Idx)) return nullptr;
	return Entries[Idx].EmissiveMaterial.LoadSynchronous();
}

bool UTileMaterialRegistry::Validate(TArray<FString>& OutErrors) const
{
	bool bOk = true;
	const int32 ExpectedCount = static_cast<int32>(EMaterialType::Count);

	if (Entries.Num() != ExpectedCount)
	{
		OutErrors.Add(FString::Printf(
			TEXT("UTileMaterialRegistry: Entries 數量 %d，應為 %d（EMaterialType::Count）"),
			Entries.Num(), ExpectedCount));
		bOk = false;
	}

	for (int32 i = 1; i < ExpectedCount; ++i) // i=0 是 Air，不需要材質
	{
		if (!Entries.IsValidIndex(i) || !Entries[i].IsValid())
		{
			OutErrors.Add(FString::Printf(
				TEXT("EMaterialType[%d] 缺少 SurfaceMaterial"), i));
			bOk = false;
		}
	}
	return bOk;
}
