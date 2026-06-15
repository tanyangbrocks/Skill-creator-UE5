#pragma once
#include "CoreMinimal.h"
#include "ManaType.h"
#include "ElementType.h"
#include "SpellArray.generated.h"

// 容器施放方式（對應 Godot ContainerType.cs）
UENUM(BlueprintType)
enum class EContainerType : uint8
{
    DirectCast   UMETA(DisplayName="直接施放"),
    Projectile   UMETA(DisplayName="投射物"),
    Contact      UMETA(DisplayName="接觸命中"),  // 即時近戰：前方掃描 + 元素 CA
    Summon       UMETA(DisplayName="召喚物"),
    Area         UMETA(DisplayName="區域"),
};

// 刻印資料（精簡版，M-4 角色系統補完）
USTRUCT(BlueprintType)
struct FEngraveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName EngraveId;
    UPROPERTY(EditAnywhere) int32 Points = 0;
};

// 技能因子插槽（對應 Godot SpellSlot.cs）
USTRUCT(BlueprintType)
struct FSpellSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName          Name;            // 插槽名稱（空 = 自動命名 slot_N）
    UPROPERTY(EditAnywhere) FName          TotemId;         // 技能因子 ID（空 = 插槽為空）
    UPROPERTY(EditAnywhere) FName          ManaTypeKey;     // MP 類型 key（None = 未指定）
    UPROPERTY(EditAnywhere) TArray<FEngraveData> LocalEngravings;

    bool IsEmpty() const { return TotemId.IsNone(); }
};

// 技能整構（對應 Godot SpellArray.cs）
// 玩家設計的完整能力單元：插槽 + 積木 + 全域刻印 + 設定
USTRUCT(BlueprintType)
struct FSpellArray
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FString                Name;
    UPROPERTY(EditAnywhere) TArray<FSpellSlot>     Slots;
    UPROPERTY(EditAnywhere) TArray<FEngraveData>   GlobalEngravings;
    UPROPERTY(EditAnywhere) EAbilityActivationType ActivationType = EAbilityActivationType::None;
    UPROPERTY(EditAnywhere) EContainerType         Container      = EContainerType::DirectCast;
    UPROPERTY(EditAnywhere) float                  CastDelay      = 0.3f;
    UPROPERTY(EditAnywhere) float                  BaseMpCost     = 0.f;
    UPROPERTY(EditAnywhere) FString                NextInCombo;   // 空字串 = 連段終止
    UPROPERTY(EditAnywhere) int32                  SceneUseLimit  = 0; // 0 = 無限制
    UPROPERTY(EditAnywhere) ESkillElementType      SpellElement   = ESkillElementType::None; // Contact/Projectile 命中元素

    // 容器效果（最深 SafetyGuard::MaxContainerDepth 層）
    // TUniquePtr 不能加 UPROPERTY → 用 TSharedPtr 讓 BP/序列化可見
    TSharedPtr<FSpellArray> ContainerEffect;

    // W-6B：每個技能整構最多可使用的 MP 種類上限
    static constexpr int32 MaxManaTypes = 3;

    bool IsValid() const { return !Name.IsEmpty() && Slots.ContainsByPredicate([](const FSpellSlot& S){ return !S.IsEmpty(); }); }

    // 積木 AST 根節點（SpellCompiler 編譯後填入，載入時從 JSON 反序列化）
    // 不能 UPROPERTY — TUniquePtr<FBlockNode> 不支援 UHT 反射
    // Blocks 欄位在執行期由 SpellCompiler 結果覆蓋
};
