#pragma once
#include "CoreMinimal.h"
#include "ManaType.h"

// 所有 MP 類型的靜態登記表（對應 Godot ManaTypeRegistry.cs）
// W-6A：18 種基礎類型（三大根源各 6）
// W-13：複合類型（53 種）呼叫 Register() 追加，不需改動此類
class ABILITYSYSTEM_API FManaTypeRegistry
{
public:
    // 取得單例（首次呼叫時初始化 18 種基礎類型）
    static FManaTypeRegistry& Get();

    // 依 key 查詢（找不到回傳 nullptr）
    const FManaType* Find(FName Key) const;

    // 依 SortOrder 排序，供 HUD 顯示用
    TArray<const FManaType*> GetSortedForHud() const;

    // W-13 追加複合類型時使用
    void Register(FManaType Type);

    // 同根源判斷
    bool AreSameRoot(FName KeyA, FName KeyB) const;

    const TMap<FName, FManaType>& GetAll() const { return All; }

private:
    FManaTypeRegistry();   // 建構時登記 18 種基礎類型

    TMap<FName, FManaType> All;
};
