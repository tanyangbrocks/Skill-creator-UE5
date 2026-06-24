// ══════════════════════════════════════════════════════════════════════
//  NPCBrainTests — UE5 Automation Tests（引擎內跑，不需 Editor，不需 llama-server）
//
//  只覆蓋不依賴真實 LLM / 真實世界的純邏輯部分：
//    - FNPCIdentity 的 JSON 存讀（M-NPC-1）
//    - UNPCMemoryComponent 的緩衝區管理規則（M-NPC-2）—— 走「無 Brain subsystem」
//      的 fallback 路徑，驗證緩衝區行為本身，不驗證真正的 LLM 摘要品質
//    - FNPCPerceptionLogic 的感知變化描述（M-NPC-3）
//
//  執行方式：
//    Editor 內：Window -> Test Automation -> "NPCBrain.*"
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NPCIdentity.h"
#include "UNPCMemoryComponent.h"
#include "WorldSnapshot.h"
#include "HAL/PlatformFilemanager.h"

// ══════════════════════════════════════════════════════════════════
//  T01 — FNPCIdentity Save/Load JSON 往返
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBrainTest_IdentityRoundTrip,
	"NPCBrain.Identity.SaveLoadRoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCBrainTest_IdentityRoundTrip::RunTest(const FString&)
{
	const FName TestId = TEXT("UnitTest_Identity_RoundTrip");

	FNPCIdentity Original;
	Original.NPCId      = TestId;
	Original.Name       = TEXT("測試村民");
	Original.Species    = TEXT("人族");
	Original.Faction     = TEXT("自由鎮");
	Original.Role        = TEXT("鐵匠");
	Original.Traits.Add(TEXT("固執"));
	Original.Traits.Add(TEXT("健談"));
	Original.SpeechStyle = TEXT("粗聲粗氣");
	Original.Backstory   = TEXT("年輕時是傭兵,退役後定居於此開了鐵匠鋪。");

	TestTrue(TEXT("Save() 成功"), Original.Save());

	FNPCIdentity Loaded;
	TestTrue(TEXT("Load() 成功"), FNPCIdentity::Load(TestId, Loaded));

	TestEqual(TEXT("NPCId 一致"), Loaded.NPCId, Original.NPCId);
	TestEqual(TEXT("Name 一致"), Loaded.Name, Original.Name);
	TestEqual(TEXT("Species 一致"), Loaded.Species, Original.Species);
	TestEqual(TEXT("Faction 一致"), Loaded.Faction, Original.Faction);
	TestEqual(TEXT("Role 一致"), Loaded.Role, Original.Role);
	TestEqual(TEXT("Traits 數量一致"), Loaded.Traits.Num(), Original.Traits.Num());
	TestEqual(TEXT("SpeechStyle 一致"), Loaded.SpeechStyle, Original.SpeechStyle);
	TestEqual(TEXT("Backstory 一致"), Loaded.Backstory, Original.Backstory);
	TestTrue(TEXT("IsValid() == true"), Loaded.IsValid());

	// 清理測試檔案，不留垃圾
	IFileManager::Get().Delete(*FNPCIdentity::FilePath(TestId));

	return true;
}

// ══════════════════════════════════════════════════════════════════
//  T02 — UNPCMemoryComponent：永久事件不受短期緩衝區淘汰影響
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBrainTest_MemoryPermanentPinning,
	"NPCBrain.Memory.PermanentEventsNeverEvicted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCBrainTest_MemoryPermanentPinning::RunTest(const FString&)
{
	UNPCMemoryComponent* Memory = NewObject<UNPCMemoryComponent>();

	Memory->AddMemoryEvent(ENPCMemoryCategory::World, TEXT("一名外鄉人來到了村莊"), /*bPermanent=*/true);

	// 灌爆短期緩衝區（cap=20），觸發壓縮（無 Brain subsystem → fallback 路徑）
	for (int32 i = 0; i < UNPCMemoryComponent::ShortTermMemoryCap; ++i)
	{
		Memory->AddMemoryEvent(ENPCMemoryCategory::Observe, FString::Printf(TEXT("瑣事 %d"), i));
	}

	TestEqual(TEXT("永久記憶區仍有 1 條"), Memory->GetPermanentMemory().Num(), 1);
	TestEqual(TEXT("短期緩衝區觸發壓縮後已清空"), Memory->GetShortTermMemory().Num(), 0);
	TestFalse(TEXT("長期摘要非空（fallback 串接）"), Memory->GetLongTermSummary().IsEmpty());
	TestFalse(TEXT("壓縮已完成，不再 in-flight"), Memory->IsCompressionInFlight());

	return true;
}

// ══════════════════════════════════════════════════════════════════
//  T03 — UNPCMemoryComponent：未達上限前不觸發壓縮
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBrainTest_MemoryNoCompressionBelowCap,
	"NPCBrain.Memory.NoCompressionBelowCap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCBrainTest_MemoryNoCompressionBelowCap::RunTest(const FString&)
{
	UNPCMemoryComponent* Memory = NewObject<UNPCMemoryComponent>();

	for (int32 i = 0; i < UNPCMemoryComponent::ShortTermMemoryCap - 1; ++i)
	{
		Memory->AddMemoryEvent(ENPCMemoryCategory::Observe, FString::Printf(TEXT("事件 %d"), i));
	}

	TestEqual(TEXT("短期緩衝區尚未清空（未達上限）"),
		Memory->GetShortTermMemory().Num(), UNPCMemoryComponent::ShortTermMemoryCap - 1);
	TestTrue(TEXT("長期摘要仍為空（尚未觸發壓縮）"), Memory->GetLongTermSummary().IsEmpty());

	return true;
}

// ══════════════════════════════════════════════════════════════════
//  T04 — FNPCPerceptionLogic：生物進出感知範圍的描述
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBrainTest_PerceptionCreatureDiff,
	"NPCBrain.Perception.CreatureAppearAndLeave",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCBrainTest_PerceptionCreatureDiff::RunTest(const FString&)
{
	FNPCWorldSnapshot Prev;
	Prev.NearbyCreatureIds.Add(1);
	Prev.NearbyCreatureIds.Add(2);

	FNPCWorldSnapshot Current;
	Current.NearbyCreatureIds.Add(2);
	Current.NearbyCreatureIds.Add(3); // 1 離開，3 新出現

	const TArray<FString> Changes = FNPCPerceptionLogic::DescribeChanges(Prev, Current);

	TestEqual(TEXT("產生 2 條描述（新出現 + 離開）"), Changes.Num(), 2);

	bool bFoundAppear = false, bFoundLeave = false;
	for (const FString& S : Changes)
	{
		if (S.Contains(TEXT("新接近"))) bFoundAppear = true;
		if (S.Contains(TEXT("不再出現"))) bFoundLeave = true;
	}
	TestTrue(TEXT("包含新接近描述"), bFoundAppear);
	TestTrue(TEXT("包含離開描述"), bFoundLeave);

	// 無變化時應回傳空陣列
	const TArray<FString> NoChange = FNPCPerceptionLogic::DescribeChanges(Prev, Prev);
	TestEqual(TEXT("快照相同時無描述"), NoChange.Num(), 0);

	return true;
}

// ══════════════════════════════════════════════════════════════════
//  T05 — FNPCPerceptionLogic：危險材質出現/消退 + 分類正確性
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBrainTest_PerceptionHazardDiff,
	"NPCBrain.Perception.HazardAppearAndClear",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCBrainTest_PerceptionHazardDiff::RunTest(const FString&)
{
	TestTrue(TEXT("Lava 是危險材質"), FNPCPerceptionLogic::IsHazardMaterial(EMaterialType::Lava));
	TestTrue(TEXT("Fire 是危險材質"), FNPCPerceptionLogic::IsHazardMaterial(EMaterialType::Fire));
	TestFalse(TEXT("Stone 不是危險材質"), FNPCPerceptionLogic::IsHazardMaterial(EMaterialType::Stone_Cobble));
	TestFalse(TEXT("Water 不是危險材質"), FNPCPerceptionLogic::IsHazardMaterial(EMaterialType::Water));

	FNPCWorldSnapshot Prev;
	Prev.HazardMaterials = { EMaterialType::Lava };

	FNPCWorldSnapshot Current;
	Current.HazardMaterials = { EMaterialType::Fire }; // 熔岩消退，火焰出現

	const TArray<FString> Changes = FNPCPerceptionLogic::DescribeChanges(Prev, Current);
	TestEqual(TEXT("產生 2 條描述（出現 + 消退）"), Changes.Num(), 2);

	bool bFoundFireAppear = false, bFoundLavaClear = false;
	for (const FString& S : Changes)
	{
		if (S.Contains(TEXT("火焰")) && S.Contains(TEXT("出現"))) bFoundFireAppear = true;
		if (S.Contains(TEXT("熔岩")) && S.Contains(TEXT("消退"))) bFoundLavaClear = true;
	}
	TestTrue(TEXT("包含火焰出現描述"), bFoundFireAppear);
	TestTrue(TEXT("包含熔岩消退描述"), bFoundLavaClear);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
