// ══════════════════════════════════════════════════════════════════════
//  SkillCreatorCoreTests — UE5 Automation Tests（引擎內跑，不需 Editor）
//
//  執行方式：
//    Editor 內：Window -> Test Automation -> "SkillCreatorCore.*"
//    命令列（CI）：
//      UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests SkillCreatorCore; Quit"
//        -unattended -nopause -nullrhi -log
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GridPos.h"
#include "ItemRegistry.h"
#include "ItemId.h"

// ══════════════════════════════════════════════════════════════════
//  T01 — FGridPos.ChebyshevDistance 對角線 = 1 tile（L11 bug fix 驗證）
//
//  L11 修復前：UBTTask_AttackPlayer 用 ManhattanDistance，斜角相鄰格為距離 2，
//  導致攻擊範圍判斷在 45° 對角線方向多消耗一格「追擊步數」。
//  這個測試確認 ChebyshevDistance 對角線正確回傳 1，而不是 2。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCreatorCoreTest_ChebyshevDiagonal,
    "SkillCreatorCore.GridPos.ChebyshevDiagonal",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSkillCreatorCoreTest_ChebyshevDiagonal::RunTest(const FString&)
{
    const FGridPos Origin(0, 0, 0);

    // XY 平面對角：Manhattan=2，但 Chebyshev 應為 1
    const FGridPos Diag2D(1, 1, 0);
    TestEqual(TEXT("XY對角（棋盤格距離）= 1"),  Origin.ChebyshevDistance(Diag2D), 1);
    TestEqual(TEXT("XY對角（曼哈頓距離）= 2"),   Origin.ManhattanDistance(Diag2D), 2);

    // 3D 全對角：Manhattan=3，但 Chebyshev 應為 1
    const FGridPos Diag3D(1, 1, 1);
    TestEqual(TEXT("3D對角（棋盤格距離）= 1"),   Origin.ChebyshevDistance(Diag3D), 1);
    TestEqual(TEXT("3D對角（曼哈頓距離）= 3"),   Origin.ManhattanDistance(Diag3D), 3);

    // 直線相鄰：兩者都是 1
    const FGridPos StraightX(1, 0, 0);
    TestEqual(TEXT("直線（棋盤格距離）= 1"), Origin.ChebyshevDistance(StraightX), 1);
    TestEqual(TEXT("直線（曼哈頓距離）= 1"), Origin.ManhattanDistance(StraightX), 1);

    // 同點：距離 = 0
    TestEqual(TEXT("自身距離 = 0"), Origin.ChebyshevDistance(Origin), 0);

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T02 — FGridPos.ChebyshevDistance 最大軸主導（混合距離）
//
//  Chebyshev 距離 = max(|dX|, |dY|, |dZ|)。
//  確認當各軸不同時，取最大那個軸的值。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCreatorCoreTest_ChebyshevMaxAxis,
    "SkillCreatorCore.GridPos.ChebyshevMaxAxis",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSkillCreatorCoreTest_ChebyshevMaxAxis::RunTest(const FString&)
{
    const FGridPos Origin(0, 0, 0);

    // |dX|=3, |dY|=1, |dZ|=2 → Chebyshev = 3
    TestEqual(TEXT("max(3,1,2) = 3"), Origin.ChebyshevDistance(FGridPos(3, 1, 2)), 3);

    // |dX|=2, |dY|=4, |dZ|=1 → Chebyshev = 4
    TestEqual(TEXT("max(2,4,1) = 4"), Origin.ChebyshevDistance(FGridPos(2, 4, 1)), 4);

    // 負方向：絕對值相同
    TestEqual(TEXT("負方向對稱"), FGridPos(5, 5, 5).ChebyshevDistance(FGridPos(3, 3, 3)), 2);

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T03 — FItemRegistry 包含 OreGoldRaw（L13 bug fix 驗證）
//
//  L13 修復前：EItemId 缺少 OreGoldRaw，金礦採掘掉落為 None（空）。
//  這個測試確認：
//    1. FItemRegistry::Get(OreGoldRaw) 回傳 Id = OreGoldRaw（不是 None）
//    2. DisplayName 非空（表示 ItemRegistry.cpp 有填入該條目）
//    3. MaxStack > 1（是可堆疊原材料，不是單一裝備）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCreatorCoreTest_OreGoldRawItem,
    "SkillCreatorCore.ItemRegistry.OreGoldRaw",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSkillCreatorCoreTest_OreGoldRawItem::RunTest(const FString&)
{
    const FItemData& Item = FItemRegistry::Get(EItemId::OreGoldRaw);

    TestEqual(TEXT("Id == OreGoldRaw"), Item.Id, EItemId::OreGoldRaw);
    TestFalse(TEXT("DisplayName 非空"), Item.DisplayName.IsEmpty());
    TestTrue(TEXT("MaxStack > 1（可堆疊原材料）"), Item.MaxStack > 1);
    TestFalse(TEXT("不是工具"), Item.bIsTool);
    TestFalse(TEXT("不是可放置方塊"), Item.bIsPlaceable);

    UE_LOG(LogTemp, Log, TEXT("[SkillCreatorCoreTest] OreGoldRaw DisplayName = %s"),
        *Item.DisplayName.ToString());

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T04 — FItemRegistry 所有非 None 條目 Id 一致性
//
//  確認 EItemId 枚舉的每個值（除 None 和 COUNT）在 ItemRegistry
//  中都有對應條目，且條目的 Id 與查詢 key 一致（防止 Set() 順序錯誤）。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCreatorCoreTest_ItemRegistryConsistency,
    "SkillCreatorCore.ItemRegistry.Consistency",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSkillCreatorCoreTest_ItemRegistryConsistency::RunTest(const FString&)
{
    int32 MismatchCount = 0;
    const int32 Count = (int32)EItemId::COUNT;

    for (int32 i = 1; i < Count; ++i)   // 從 1 跳過 None
    {
        EItemId ExpectedId = (EItemId)i;
        const FItemData& D = FItemRegistry::Get(ExpectedId);

        if (D.Id != ExpectedId)
        {
            AddError(FString::Printf(
                TEXT("ItemRegistry[%d] Id 不一致：期望 %d，實際 %d"),
                i, (int32)ExpectedId, (int32)D.Id));
            ++MismatchCount;
        }
    }

    TestEqual(TEXT("所有條目 Id 一致（0 個不符）"), MismatchCount, 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
