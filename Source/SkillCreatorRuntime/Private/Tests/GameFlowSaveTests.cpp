// ══════════════════════════════════════════════════════════════════════
//  GameFlowSaveTests — UE5 Automation Tests（引擎內跑，不需 Editor）
//
//  覆蓋 2026-06-19 角色獨立存檔重構（FCharacterSaveData 從綁定 WorldDir
//  改為獨立的全域角色清單，對應 Godot 原版的角色/世界自由搭配）。
//  只測資料層（存讀/清單/刪除），不測 UGameFlowWidget 的畫面渲染——背景
//  測試視窗的 Slate 量測在這個專案已證實不可靠，資料層才是能可靠驗證的部分。
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CharacterSaveData.h"
#include "FlowSaveSystem.h"
#include "HAL/FileManager.h"

// ══════════════════════════════════════════════════════════════════
//  T01 — FCharacterSaveData Save/Load 往返（Id-based，不綁定任何世界）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFlowSaveTest_CharacterRoundTrip,
    "SkillCreatorRuntime.GameFlow.CharacterSaveRoundTrip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGameFlowSaveTest_CharacterRoundTrip::RunTest(const FString&)
{
    const FString TestId = TEXT("UnitTest_CharacterSaveData_RoundTrip");

    FCharacterSaveData Original;
    Original.Id            = TestId;
    Original.CharacterName = TEXT("測試旅者");
    Original.Level         = 5;
    Original.Xp            = 123.f;
    Original.CurrentHp     = 80.f;
    Original.CurrentMp     = 40.f;

    TestTrue(TEXT("Save() 成功"), Original.Save());

    FCharacterSaveData Loaded;
    TestTrue(TEXT("Load() 成功"), FCharacterSaveData::Load(TestId, Loaded));

    TestEqual(TEXT("Id 一致"), Loaded.Id, Original.Id);
    TestEqual(TEXT("CharacterName 一致"), Loaded.CharacterName, Original.CharacterName);
    TestEqual(TEXT("Level 一致"), Loaded.Level, Original.Level);
    TestEqual(TEXT("Xp 一致"), Loaded.Xp, Original.Xp);
    TestEqual(TEXT("CurrentHp 一致"), Loaded.CurrentHp, Original.CurrentHp);

    IFileManager::Get().Delete(*FCharacterSaveData::FilePath(TestId));
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T02 — FFlowSaveSystem 角色清單：建立 → 出現在 ListAllCharacters → 刪除 → 消失
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFlowSaveTest_CharacterRosterLifecycle,
    "SkillCreatorRuntime.GameFlow.CharacterRosterLifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGameFlowSaveTest_CharacterRosterLifecycle::RunTest(const FString&)
{
    FCharacterSaveData NewChar;
    TestTrue(TEXT("CreateNewCharacter 成功"),
        FFlowSaveSystem::CreateNewCharacter(TEXT("稽核測試角色"), NewChar));
    TestFalse(TEXT("自動生成的 Id 非空"), NewChar.Id.IsEmpty());

    TArray<FCharacterSaveData> All;
    FFlowSaveSystem::ListAllCharacters(All);
    const bool bFoundAfterCreate = All.ContainsByPredicate(
        [&NewChar](const FCharacterSaveData& C){ return C.Id == NewChar.Id; });
    TestTrue(TEXT("建立後出現在 ListAllCharacters"), bFoundAfterCreate);

    // 角色獨立於世界：兩個角色互不相干，可各自存在
    FCharacterSaveData SecondChar;
    FFlowSaveSystem::CreateNewCharacter(TEXT("第二個測試角色"), SecondChar);
    TestNotEqual(TEXT("兩個角色 Id 不同"), NewChar.Id, SecondChar.Id);

    TestTrue(TEXT("DeleteCharacter(NewChar) 成功"), FFlowSaveSystem::DeleteCharacter(NewChar.Id));

    TArray<FCharacterSaveData> AfterDelete;
    FFlowSaveSystem::ListAllCharacters(AfterDelete);
    const bool bStillThere = AfterDelete.ContainsByPredicate(
        [&NewChar](const FCharacterSaveData& C){ return C.Id == NewChar.Id; });
    TestFalse(TEXT("刪除後不再出現在 ListAllCharacters"), bStillThere);

    const bool bSecondStillThere = AfterDelete.ContainsByPredicate(
        [&SecondChar](const FCharacterSaveData& C){ return C.Id == SecondChar.Id; });
    TestTrue(TEXT("刪除第一個角色不影響第二個角色"), bSecondStillThere);

    // 清理
    FFlowSaveSystem::DeleteCharacter(SecondChar.Id);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
