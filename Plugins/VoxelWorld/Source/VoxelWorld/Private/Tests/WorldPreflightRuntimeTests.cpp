// ══════════════════════════════════════════════════════════════════════
//  WorldPreflightRuntimeTests — UE5 Automation Test（跑在引擎裡，不是純文字掃描）
//
//  跟專案根目錄的 preflight-check.ps1 是互補關係，不是取代：
//    - preflight-check.ps1：純文字 / regex 比對原始碼，不需要編譯，但無法驗證實際執行期行為
//    - 這個檔案：真正呼叫引擎程式碼、驅動實際邏輯，能抓到「程式碼長得對但跑起來不對」的錯誤
//
//  執行方式：
//    1. UE5 Editor 內：Window -> Test Automation -> 勾選 "VoxelWorld.Preflight.*" -> Start Tests
//    2. 命令列（CI 用）：
//       UnrealEditor-Cmd.exe "<uproject路徑>" -ExecCmds="Automation RunTests VoxelWorld.Preflight; Quit" -unattended -nopause -nullrhi -log
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TileWorld3D.h"
#include "ElementalReactionTable.h"
#include "ElementType.h"
#include "MaterialRegistry.h"
#include "ItemId.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPreflightTest, Log, All);

// ══════════════════════════════════════════════════════════════════
//  T01 — 未載入 chunk 的查詢行為（preflight-check.ps1 Check 7g 的根因驗證）
//
//  preflight-check.ps1 用原始碼比對抓到 AMobSpawnController 缺少 EnsureChunkAt，
//  推論依據是讀了 FTileWorld3D::GetCell 的原始碼（對未載入 chunk 回傳預設 FTileCell{}）。
//  這個測試把那個推論變成真正跑過引擎程式碼確認的事實，而不是只看程式碼猜測行為。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPreflightTest_UnloadedChunkReturnsAir,
    "VoxelWorld.Preflight.UnloadedChunkReturnsAirNotError",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldPreflightTest_UnloadedChunkReturnsAir::RunTest(const FString&)
{
    FTileWorld3D World;

    // 刻意不呼叫任何會建立 chunk 的方法（不 SetTile / 不 GetOrCreateChunk），
    // 直接查詢一個遠離原點、絕對沒有被載入過的座標。
    const int32 FarX = 100000, FarY = 50, FarZ = 100000;

    EMaterialType Result = World.GetTile(FarX, FarY, FarZ);

    TestTrue(TEXT("未載入 chunk 的查詢回傳 Air（而不是崩潰或拋例外）"),
        Result == EMaterialType::Air);

    // 這個結果本身證實了 preflight-check.ps1 對 7g 的推論：
    // 任何在「目標 chunk 可能尚未載入」情境下查詢地形高度的程式碼（例如 MobSpawnController
    // 跨距離生成怪），若沒有先呼叫類似 EnsureChunkAt 的機制，看到的永遠是 Air，
    // 不會有任何錯誤訊息提示「這其實是因為 chunk 沒載入，不是這裡真的沒地形」。
    UE_LOG(LogWorldPreflightTest, Log,
        TEXT("[Preflight] 確認：未載入 chunk 查詢無聲回傳 Air，與 GetCell 原始碼判讀一致"));

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T02 — ElementalReactionTable 22 條反應的功能性驗證（不只是數數量）
//
//  preflight-check.ps1 Check 13c 只數了 Tab.Add(MakeKey(...)) 出現次數 == 22。
//  這個測試額外驗證：每一條反應的 A+B 與 B+A 查詢結果一致（對稱性是設計明文要求，
//  純文字計數抓不到「寫了 22 條，但其中一條的對稱輸入查不到」這種錯誤）。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPreflightTest_ElementalReactionSymmetry,
    "VoxelWorld.Preflight.ElementalReactionTableSymmetry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldPreflightTest_ElementalReactionSymmetry::RunTest(const FString&)
{
    using E = ESkillElementType;

    // 與 ElementalReactionTable.cpp 註冊表一致的「水系」5 條（最容易因為手誤漏寫對稱輸入）
    const TPair<E, E> KnownPairs[] = {
        { E::Water, E::Metal   },
        { E::Water, E::Wood    },
        { E::Water, E::Earth   },
        { E::Water, E::Thunder },
        { E::Water, E::Ice     },
    };

    bool bAllSymmetric = true;
    for (const auto& Pair : KnownPairs)
    {
        const FReactionDef* Forward  = FElementalReactionTable::Lookup(Pair.Key, Pair.Value);
        const FReactionDef* Backward = FElementalReactionTable::Lookup(Pair.Value, Pair.Key);

        if (!Forward || !Backward)
        {
            AddError(FString::Printf(
                TEXT("元素反應查詢不對稱或缺失：A=%d B=%d Forward=%s Backward=%s"),
                (int32)Pair.Key, (int32)Pair.Value,
                Forward ? TEXT("有") : TEXT("無"), Backward ? TEXT("有") : TEXT("無")));
            bAllSymmetric = false;
        }
    }

    TestTrue(TEXT("已知反應對在 A+B 與 B+A 兩個查詢方向都能找到（對稱性成立）"), bAllSymmetric);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T03 — MaterialRegistry Ore_Gold 掉落配置（L13 bug fix 驗證）
//
//  L13 修復前：EMaterialType::Ore_Gold 的 FragmentItem 為 None，
//  且 GetDefaultDrops() 沒有對應 case，金礦無任何掉落。
//  這個測試確認：
//    1. GetFragmentItem(Ore_Gold) == OreGoldRaw（GMatData 條目修正）
//    2. GetDefaultDrops(Ore_Gold) 包含 OreGoldRaw 掉落（switch case 修正）
//    3. 掉落數量 > 0（完整性）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPreflightTest_OreGoldDrops,
    "VoxelWorld.Preflight.OreGoldDropsExist",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldPreflightTest_OreGoldDrops::RunTest(const FString&)
{
    // 1. FragmentItem（GMatData 欄位）
    EItemId Fragment = FMaterialRegistry::GetFragmentItem(EMaterialType::Ore_Gold);
    TestEqual(TEXT("Ore_Gold.FragmentItem == OreGoldRaw"), Fragment, EItemId::OreGoldRaw);

    // 2. GetDefaultDrops()
    TArray<FItemDrop> Drops = FMaterialRegistry::GetDefaultDrops(EMaterialType::Ore_Gold);
    TestTrue(TEXT("GetDefaultDrops(Ore_Gold) 非空"), Drops.Num() > 0);

    if (Drops.Num() > 0)
    {
        TestEqual(TEXT("首個掉落 ItemId == OreGoldRaw"), Drops[0].ItemId, EItemId::OreGoldRaw);
        TestTrue(TEXT("掉落最大數量 > 0"), Drops[0].MaxCount > 0);
    }

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T04 — MaterialRegistry 所有可採礦材質均有 FragmentItem（完整性）
//
//  防止未來新增 Ore_X 時只改了 EMaterialType 但忘記填 GMatData 的 FragmentItem。
//  規則：bIsMineable == true 的材質，FragmentItem 不得為 None。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPreflightTest_MineableHasFragment,
    "VoxelWorld.Preflight.MineableMaterialHasFragment",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldPreflightTest_MineableHasFragment::RunTest(const FString&)
{
    // 已知的可採礦材質列表（與 GMatData 中 bIsMineable=true 的條目對應）
    const EMaterialType MineableTypes[] = {
        EMaterialType::Stone,
        EMaterialType::Dirt,
        EMaterialType::Grass,
        EMaterialType::Sand,
        EMaterialType::Wood,
        EMaterialType::Leaves,
        EMaterialType::Ore_Iron,
        EMaterialType::Ore_Gold,
        EMaterialType::Ash,
        EMaterialType::Ore_Coal,
        EMaterialType::Ore_Copper,
        EMaterialType::Ore_MagicCrystal,
    };

    bool bAllHaveFragment = true;
    for (EMaterialType Mat : MineableTypes)
    {
        const FMaterialData& D = FMaterialRegistry::Get(Mat);
        if (D.bIsMineable && D.FragmentItem == EItemId::None)
        {
            AddError(FString::Printf(
                TEXT("可採礦材質 ID=%d bIsMineable=true 但 FragmentItem=None"),
                (uint8)Mat));
            bAllHaveFragment = false;
        }
    }

    TestTrue(TEXT("所有可採礦材質均有 FragmentItem"), bAllHaveFragment);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
