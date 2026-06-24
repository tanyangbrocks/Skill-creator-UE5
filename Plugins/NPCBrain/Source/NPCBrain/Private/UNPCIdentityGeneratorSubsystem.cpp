#include "UNPCIdentityGeneratorSubsystem.h"
#include "UNPCBrainSubsystem.h"
#include "NPCBrainSettings.h"
#include "JsonObjectConverter.h"
#include "RaceRegistry.h"

UNPCIdentityGeneratorSubsystem* UNPCIdentityGeneratorSubsystem::Get(const UObject* WorldCtx)
{
	if (!WorldCtx) return nullptr;
	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldCtx, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UNPCIdentityGeneratorSubsystem>() : nullptr;
}

bool UNPCIdentityGeneratorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UNPCIdentityGeneratorSubsystem::LoadOrGenerate(FName NPCId, FName SubtypeId, FOnIdentityReady OnReady)
{
	FNPCIdentity Existing;
	if (FNPCIdentity::Load(NPCId, Existing))
	{
		OnReady.ExecuteIfBound(Existing);
		return;
	}

	RequestGeneration(NPCId, SubtypeId, OnReady);
}

FName UNPCIdentityGeneratorSubsystem::PickRandomRaceId()
{
	// 不再讓 LLM 自由發明種族名稱：直接從 FRaceRegistry（W-10 角色創建用，164 種）
	// 隨機抽一個，跟玩家角色創建系統共用同一份種族表（docs/plan-base-npc-system.md §三）。
	TArray<FName> AllRaceIds;
	for (const FRaceSystemDefinition& System : FRaceRegistry::AllSystems())
		for (const FRaceDefinition* Race : FRaceRegistry::RacesInSystem(System.SystemId))
			if (Race) AllRaceIds.Add(Race->Id);

	if (AllRaceIds.IsEmpty()) return NAME_None;
	return AllRaceIds[FMath::RandRange(0, AllRaceIds.Num() - 1)];
}

void UNPCIdentityGeneratorSubsystem::RequestGeneration(FName NPCId, FName SubtypeId, FOnIdentityReady OnReady)
{
	UNPCBrainSubsystem* Brain = UNPCBrainSubsystem::Get(this);
	if (!Brain)
	{
		UE_LOG(LogTemp, Error,
			TEXT("NPCBrain: cannot generate identity for '%s' — brain subsystem unavailable"),
			*NPCId.ToString());
		return;
	}

	const FName RaceId = PickRandomRaceId();
	const FRaceDefinition* RaceDef = FRaceRegistry::Find(RaceId);

	const UNPCBrainSettings* S = UNPCBrainSettings::Get();

	TArray<FNPCMessage> Messages;
	FNPCMessage Sys;
	Sys.Role    = ENPCMessageRole::System;
	Sys.Content = S->WorldviewSystemPrompt;
	Messages.Add(Sys);

	// 把抽到的種族設定餵進 prompt，讓生成的背景故事符合該種族的既定描述
	// （docs/plan-base-npc-system.md §三第 3 點）
	FNPCMessage User;
	User.Role = ENPCMessageRole::User;
	User.Content = RaceDef
		? FString::Printf(TEXT("請生成一位新 NPC 的身分。這名 NPC 的種族是「%s」：%s"),
			*RaceDef->DisplayName.ToString(), *RaceDef->Description.ToString())
		: TEXT("請生成一位新 NPC 的身分。");
	Messages.Add(User);

	FOnInferenceComplete OnComplete;
	OnComplete.BindLambda([this, NPCId, SubtypeId, RaceId, OnReady](const FString& Response)
	{
		ParseAndSave(NPCId, SubtypeId, RaceId, Response, OnReady);
	});

	FOnInferenceError OnError;
	OnError.BindLambda([NPCId](int32 Code, const FString& Err)
	{
		UE_LOG(LogTemp, Error,
			TEXT("NPCBrain: identity generation failed for '%s': [%d] %s"),
			*NPCId.ToString(), Code, *Err);
	});

	Brain->SendMessages(Messages, OnComplete, OnError);
}

void UNPCIdentityGeneratorSubsystem::ParseAndSave(
	FName NPCId, FName SubtypeId, FName RaceId, const FString& LlmResponseJson, FOnIdentityReady OnReady)
{
	FNPCIdentity Identity;
	Identity.NPCId     = NPCId;
	Identity.SubtypeId = SubtypeId;
	Identity.RaceId    = RaceId;

	// LLM JSON 回應不包含 npcId/subtypeId/raceId 這些 key，JsonObjectStringToUStruct 只會
	// 設定 JSON 裡實際出現的欄位，上面預先設好的三個值不會被覆寫。
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(LlmResponseJson, &Identity, 0, 0))
	{
		UE_LOG(LogTemp, Error,
			TEXT("NPCBrain: failed to parse identity JSON for '%s'. Raw response: %s"),
			*NPCId.ToString(), *LlmResponseJson);
		return;
	}

	if (!Identity.Save())
	{
		UE_LOG(LogTemp, Error, TEXT("NPCBrain: failed to save identity for '%s'"),
			*NPCId.ToString());
	}

	OnReady.ExecuteIfBound(Identity);
}
