#pragma once
#include "CoreMinimal.h"
#include "ManaType.h"
#include "ElementType.h"
#include "FBlockNode.h"
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

// 刻印顏色分類（對應 Godot EngraveColor.cs）
UENUM(BlueprintType)
enum class EEngraveColor : uint8
{
    Action    UMETA(DisplayName="行動（自動插入）"),  // 行動刻印，技能因子基礎效果自動插入
    Black     UMETA(DisplayName="黑（邏輯）"),        // 邏輯積木刻印
    White     UMETA(DisplayName="白（傷害）"),        // 傷害型
    Orange    UMETA(DisplayName="橙（控制）"),        // 控制型（擊退/減速/凍結/牽引）
    Blue      UMETA(DisplayName="藍（改造）"),        // 技能因子改造（多段/被動/不可打斷/軌跡）
    Red       UMETA(DisplayName="紅（侵略）"),        // 侵略效果（暈眩/瞬殺/斷招）
    Green     UMETA(DisplayName="綠（輔助）"),        // 輔助效果（護盾/治療/替代效果）
    Purple    UMETA(DisplayName="紫（額外操作）"),    // 額外操作（選擇型/節奏輸入）
    Yellow    UMETA(DisplayName="黃（能力限制）"),    // 能力限制
    Elemental UMETA(DisplayName="元素"),              // 屬性刻印（11 元素）
    Law       UMETA(DisplayName="法則"),              // 法則刻印（時間/空間/造化等）
};

// 刻印縮放曲線類型（對應 Godot ScalingType.cs）
UENUM(BlueprintType)
enum class EScalingType : uint8
{
    Hyperbolic UMETA(DisplayName="雙曲線（1 - 1/(1+ax)）"),  // 傷害/概率類
    Linear     UMETA(DisplayName="線性（base + x*k）"),      // 輔助類
};

// 技能因子類型（對應 Godot TotemType.cs）
UENUM(BlueprintType)
enum class ETotemType : uint8
{
    Area         UMETA(DisplayName="範圍"),       // 施放形狀（扇形/周身/遠距圓形/射線）
    Technique    UMETA(DisplayName="武技"),       // 近戰招式（劍技/拳擊/盾防）
    Projectile   UMETA(DisplayName="投射物"),     // 發射投射物
    Passive      UMETA(DisplayName="被動"),       // 持續執行
    Morph        UMETA(DisplayName="變幻"),       // 角色狀態改變
    Displacement UMETA(DisplayName="位移"),       // 衝刺/瞬移/閃避
    Summon       UMETA(DisplayName="召喚"),       // 召喚實體
    Domain       UMETA(DisplayName="領域"),       // 改變環境/地形/天候
    Custom       UMETA(DisplayName="自定義"),     // 語意由刻印決定
};

// 刻印分類（對應 Godot EngraveCategory.cs）
UENUM(BlueprintType)
enum class EEngraveCategory : uint8
{
    Action    UMETA(DisplayName="行動"),   // act_* 刻印：決定效果類型
    Modifier  UMETA(DisplayName="修飾"),   // white_/blue_/elem_/orange_/red_/yellow_
    Other     UMETA(DisplayName="其他"),
};

// 刻印觸發時機（對應 Godot EngraveTrigger.cs）
UENUM(BlueprintType)
enum class EEngraveTrigger : uint8
{
    OnCast  UMETA(DisplayName="施放時"),
    OnTick  UMETA(DisplayName="每幀"),
    OnHit   UMETA(DisplayName="命中時"),
    None    UMETA(DisplayName="無"),
};

// 刻印資料（對應 Godot EngraveData.cs）
USTRUCT(BlueprintType)
struct FEngraveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName            EngraveId;
    UPROPERTY(EditAnywhere) int32            Points      = 0;
    UPROPERTY(EditAnywhere) EEngraveColor    Color       = EEngraveColor::White;
    UPROPERTY(EditAnywhere) EEngraveCategory Category    = EEngraveCategory::Other;
    UPROPERTY(EditAnywhere) EEngraveTrigger  Trigger     = EEngraveTrigger::None;
    UPROPERTY(EditAnywhere) EScalingType     Scaling     = EScalingType::Hyperbolic;
    // 效果基礎值；CalculateEffect() 按 Points 縮放：Effect * (1 + Points * 0.1)
    UPROPERTY(EditAnywhere) float            Effect      = 0.f;

    float CalculateEffect() const { return Effect * (1.f + (float)Points * 0.1f); }
};

// 技能因子資料（對應 Godot TotemData.cs）
USTRUCT(BlueprintType)
struct FTotemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName       TotemId;                               // 唯一識別 ID
    UPROPERTY(EditAnywhere) FText       DisplayName;                           // 顯示名稱
    UPROPERTY(EditAnywhere) ETotemType  Type    = ETotemType::Custom;          // 因子類型
    UPROPERTY(EditAnywhere) int32       BaseAbilityPointCost = 10;             // 基礎能力點消耗
    UPROPERTY(EditAnywhere) int32       RequiredPlayerLevel  = 1;              // 解鎖所需玩家等級

    bool IsValid() const { return !TotemId.IsNone(); }
};

// 技能因子插槽（對應 Godot SpellSlot.cs）
USTRUCT(BlueprintType)
struct FSpellSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName          Name;            // 插槽名稱（空 = 自動命名 slot_N）
    UPROPERTY(EditAnywhere) FName          TotemId;         // 技能因子 ID（空 = 插槽為空）
    UPROPERTY(EditAnywhere) ETotemType     TotemType   = ETotemType::Custom; // 因子類型（編輯器填入）
    UPROPERTY(EditAnywhere) FName          ManaTypeKey;     // MP 類型 key（None = 未指定）
    // W-6 AbilityPointCost：放入技能因子時從 FTotemData.BaseAbilityPointCost 複製
    UPROPERTY(EditAnywhere) int32          AbilityPointCost = 0;
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

    // 是否為被動技能：任一插槽的 TotemType == Passive
    bool IsPassive() const
    {
        return Slots.ContainsByPredicate([](const FSpellSlot& S){
            return !S.IsEmpty() && S.TotemType == ETotemType::Passive;
        });
    }

    // 收集此技能整構（含容器效果）已指定的 ManaTypeKey 集合（去重）
    TSet<FName> GetUsedManaTypes(bool bRecursive = true) const
    {
        TSet<FName> Result;
        for (const FSpellSlot& S : Slots)
            if (!S.ManaTypeKey.IsNone()) Result.Add(S.ManaTypeKey);
        if (bRecursive && ContainerEffect.IsValid())
        {
            TSet<FName> Inner = ContainerEffect->GetUsedManaTypes(true);
            Result.Append(Inner);
        }
        return Result;
    }

    // MP 種類數是否在上限內（預設 MaxManaTypes = 3）
    bool IsValidManaTypeCount(int32 Limit = MaxManaTypes) const
    {
        return (int32)GetUsedManaTypes().Num() <= Limit;
    }

    // 是否有任何非空非被動插槽尚未綁定 MP 類型（對應 Godot SpellArray.HasUnboundMpBlocks）
    bool HasUnboundMpBlocks() const
    {
        for (const FSpellSlot& S : Slots)
        {
            if (S.IsEmpty()) continue;
            if (S.TotemType == ETotemType::Passive) continue;
            if (S.ManaTypeKey.IsNone()) return true;
        }
        return false;
    }

    // 主要元素（SpellElement 即為主要元素；M-5+ 刻印元素系統完善後可從 GlobalEngravings 覆蓋）
    ESkillElementType PrimaryElement() const { return SpellElement; }

    // 積木樹的 JSON 持久化字串（FSpellSaveSystem 在存讀檔時於 Blocks ↔ BlocksJson 互轉）
    UPROPERTY() FString BlocksJson;

    // 積木 AST 根節點。TUniquePtr 不可 UPROPERTY，用 TSharedPtr 包裝使 FSpellArray 保持可複製
    // （複製時共享相同的積木樹；由積木編輯器儲存後呼叫 SetBlocks() 寫入）
    TSharedPtr<TArray<TUniquePtr<FBlockNode>>> Blocks;

    void SetBlocks(TArray<TUniquePtr<FBlockNode>> InBlocks)
    {
        Blocks = MakeShared<TArray<TUniquePtr<FBlockNode>>>(MoveTemp(InBlocks));
    }
};
