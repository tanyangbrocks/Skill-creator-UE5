#include "UNPCIdentityGeneratorSubsystem.h"
#include "UNPCBrainSubsystem.h"
#include "NPCBrainSettings.h"
#include "JsonObjectConverter.h"

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

void UNPCIdentityGeneratorSubsystem::LoadOrGenerate(FName NPCId, FOnIdentityReady OnReady)
{
	FNPCIdentity Existing;
	if (FNPCIdentity::Load(NPCId, Existing))
	{
		OnReady.ExecuteIfBound(Existing);
		return;
	}

	RequestGeneration(NPCId, OnReady);
}

void UNPCIdentityGeneratorSubsystem::RequestGeneration(FName NPCId, FOnIdentityReady OnReady)
{
	UNPCBrainSubsystem* Brain = UNPCBrainSubsystem::Get(this);
	if (!Brain)
	{
		UE_LOG(LogTemp, Error,
			TEXT("NPCBrain: cannot generate identity for '%s' — brain subsystem unavailable"),
			*NPCId.ToString());
		return;
	}

	const UNPCBrainSettings* S = UNPCBrainSettings::Get();

	TArray<FNPCMessage> Messages;
	FNPCMessage Sys;
	Sys.Role    = ENPCMessageRole::System;
	Sys.Content = S->WorldviewSystemPrompt;
	Messages.Add(Sys);

	FNPCMessage User;
	User.Role    = ENPCMessageRole::User;
	User.Content = TEXT("請生成一位新 NPC 的身分。");
	Messages.Add(User);

	FOnInferenceComplete OnComplete;
	OnComplete.BindLambda([this, NPCId, OnReady](const FString& Response)
	{
		ParseAndSave(NPCId, Response, OnReady);
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
	FName NPCId, const FString& LlmResponseJson, FOnIdentityReady OnReady)
{
	FNPCIdentity Identity;
	Identity.NPCId = NPCId;

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
