#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"
#include "FBlockNode.h"
#include "Instruction.h"

// ══════════════════════════════════════════════════════════════════
//  FSpellSlotSync — 從積木樹掃描 Totem/Engraving 積木，重建
//  FSpellArray.Slots / GlobalEngravings / Container（對應 Godot
//  AbilityEditorUI.SyncSlotsFromBlocks() / SaveSpell()）。
//
//  Godot 對照：
//    C:\skill-creator\Scripts\UI\AbilityEditorUI.cs
//      SyncSlotsFromBlocks()      line 1134-1199
//      TotemToContainer()        line 1229-1237
//      HasActivationTypeBlock()  line 1261-1274
//      SaveSpell()                line 1276-1345（驗證部分 line 1282-1318）
//
//  純靜態工具類別；不依賴 UObject，可在 Slate widget 中直接呼叫。
// ══════════════════════════════════════════════════════════════════
struct FSpellSlotSync
{
    // ── Container 推導（對應 Godot TotemToContainer，line 1229-1237）──
    // 優先序：Projectile > Summon > 其他（= DirectCast）。
    // Summon 依 TotemId 細分：UE5 無 ContainerType.SummonMinion/Turret/Guardian
    // 區分（EContainerType 只有單一 Summon 值），故統一對應 Summon。
    static EContainerType TotemToContainer(const FTotemBlockArgs* Totem)
    {
        if (!Totem) return EContainerType::DirectCast;
        if (Totem->TotemType == ETotemType::Projectile) return EContainerType::Projectile;
        if (Totem->TotemType == ETotemType::Summon)      return EContainerType::Summon;
        return EContainerType::DirectCast;
    }

    // ── 從積木樹重建 Slots / GlobalEngravings / Container ──
    // 對應 Godot SyncSlotsFromBlocks()，line 1134-1199。
    // 只掃描頂層 Blocks（Godot 原版亦只掃描 _spell.Blocks 頂層，不遞迴 If/Loop 分支）。
    static void SyncSlotsFromBlocks(FSpellArray& Spell)
    {
        Spell.Slots.Reset();
        Spell.GlobalEngravings.Reset();

        const FTotemBlockArgs* FirstNonPassive = nullptr;
        FTotemBlockArgs FirstNonPassiveStorage;
        const FTotemBlockArgs* ProjectileTotem = nullptr;
        FTotemBlockArgs ProjectileStorage;
        const FTotemBlockArgs* SummonTotem = nullptr;
        FTotemBlockArgs SummonStorage;

        if (!Spell.Blocks.IsValid()) return;

        for (const TUniquePtr<FBlockNode>& BlockPtr : *Spell.Blocks)
        {
            if (!BlockPtr) continue;
            const FBlockNode& B = *BlockPtr;

            if (B.Type == EBlockType::Totem)
            {
                const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
                const FTotemBlockArgs* Args = IS ? IS->GetPtr<FTotemBlockArgs>() : nullptr;
                if (!Args || Args->TotemId.IsNone()) continue;

                FSpellSlot NewSlot;
                NewSlot.TotemId      = Args->TotemId;
                NewSlot.TotemType    = Args->TotemType;
                NewSlot.ManaTypeKey  = Args->ManaTypeKey;
                NewSlot.AbilityPointCost = Args->BaseAbilityPointCost;
                const bool bIsCustom = Args->TotemId.ToString().Equals(TEXT("custom"), ESearchCase::IgnoreCase);
                NewSlot.Name = (bIsCustom && !Args->CustomName.IsEmpty())
                    ? FName(*Args->CustomName.ToString())
                    : NAME_None;
                Spell.Slots.Add(NewSlot);

                if (!FirstNonPassive && Args->TotemType != ETotemType::Passive)
                {
                    FirstNonPassiveStorage = *Args;
                    FirstNonPassive = &FirstNonPassiveStorage;
                }
                if (!ProjectileTotem && Args->TotemType == ETotemType::Projectile)
                {
                    ProjectileStorage = *Args;
                    ProjectileTotem = &ProjectileStorage;
                }
                if (!SummonTotem && Args->TotemType == ETotemType::Summon)
                {
                    SummonStorage = *Args;
                    SummonTotem = &SummonStorage;
                }
            }
            else if (B.Type == EBlockType::Engraving)
            {
                const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
                const FEngravingBlockArgs* Args = IS ? IS->GetPtr<FEngravingBlockArgs>() : nullptr;
                if (!Args || Args->EngraveId.IsNone()) continue;

                // ⚠️ UE5 無 TotemLibrary.AllEngravings 資料表（已知缺口），
                // 無法像 Godot 查表取得 Color/Category/Trigger/ScalingType/BaseEffect。
                // 暫以積木自帶的 EngraveId + Points 建立最小可用的 FEngraveData，
                // 其餘欄位維持預設值，待 TotemLibrary 移植後補上查表邏輯。
                FEngraveData NewEngrave;
                NewEngrave.EngraveId = Args->EngraveId;
                NewEngrave.Points    = (int32)Args->Points;
                Spell.GlobalEngravings.Add(NewEngrave);
            }
        }

        // Container 優先順序：Projectile > Summon > 其他（範圍/武技等 = DirectCast）
        const FTotemBlockArgs* Chosen = ProjectileTotem ? ProjectileTotem
                                       : SummonTotem      ? SummonTotem
                                       : FirstNonPassive;
        Spell.Container = TotemToContainer(Chosen);
    }

    // ── 遞迴掃描 blocks，檢查是否有 SetActivation* 積木（含 If/Loop 內部）──
    // 對應 Godot HasActivationTypeBlock()，line 1261-1274。
    static bool HasActivationTypeBlock(const TArray<TUniquePtr<FBlockNode>>& Blocks)
    {
        for (const TUniquePtr<FBlockNode>& BlockPtr : Blocks)
        {
            if (!BlockPtr) continue;
            const FBlockNode& B = *BlockPtr;
            if (B.Type == EBlockType::SetActivationInstant
             || B.Type == EBlockType::SetActivationDeclare
             || B.Type == EBlockType::SetActivationSustained)
                return true;
            if (HasActivationTypeBlock(B.ThenBranch)) return true;
            if (HasActivationTypeBlock(B.ElseBranch)) return true;
            if (HasActivationTypeBlock(B.LoopBody))   return true;
        }
        return false;
    }

    // ── 儲存前 5 項驗證（對應 Godot SaveSpell() line 1282-1303）──
    // 回傳全部錯誤訊息（不 early-return，全部收集後一次回傳）。
    // 呼叫前必須已執行 SyncSlotsFromBlocks()（Godot SaveSpell() 第一步）。
    static TArray<FString> ValidateSpell(const FSpellArray& Spell,
                                          const TArray<TUniquePtr<FBlockNode>>& Blocks,
                                          int32 PlayerLevel)
    {
        TArray<FString> Errors;

        // ① 名稱非空白
        if (Spell.Name.TrimStartAndEnd().IsEmpty())
            Errors.Add(TEXT("請填寫技能整構名稱（必填）"));

        // ② 至少一個 Totem 積木
        bool bHasTotem = false;
        for (const TUniquePtr<FBlockNode>& BlockPtr : Blocks)
            if (BlockPtr && BlockPtr->Type == EBlockType::Totem) { bHasTotem = true; break; }

        if (!bHasTotem)
        {
            Errors.Add(TEXT("請加入技能因子來定義主被動類型"));
        }
        // ③ 主動技能必須有發動類型積木（僅在有 Totem 時才檢查，對應 Godot else-if）
        else if (!Spell.IsPassive() && !HasActivationTypeBlock(Blocks))
        {
            Errors.Add(TEXT("主動技能需要定義發動類型（請加入「即時型」、「宣告型」或「持續生效型」積木）"));
        }

        // ④ MP 類型綁定
        if (Spell.HasUnboundMpBlocks())
            Errors.Add(TEXT("有技能因子存在需消耗 MP 的積木，但尚未指定 MP 類型"));

        // ⑤ 能力點上限
        if (FAbilityPointCalculator::ExceedsLevelCap(Spell, PlayerLevel))
        {
            const int32 Ap  = FAbilityPointCalculator::CalculateTotalCost(Spell);
            const int32 Cap = FAbilityPointCalculator::TierApCap(PlayerLevel);
            Errors.Add(FString::Printf(TEXT("能力點 %d 超過境界上限 %d"), Ap, Cap));
        }

        return Errors;
    }
};
