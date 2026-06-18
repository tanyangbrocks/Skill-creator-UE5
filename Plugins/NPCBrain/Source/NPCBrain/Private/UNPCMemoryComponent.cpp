#include "UNPCMemoryComponent.h"
#include "UNPCBrainSubsystem.h"
#include "NPCBrainTypes.h"

UNPCMemoryComponent::UNPCMemoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNPCMemoryComponent::AddMemoryEvent(ENPCMemoryCategory Category, const FString& Content, bool bPermanent)
{
	FNPCMemoryEvent Event;
	Event.Timestamp  = FDateTime::Now();
	Event.Category   = Category;
	Event.Content    = Content;
	Event.bPermanent = bPermanent;

	if (bPermanent)
	{
		PermanentMemory.Add(Event);
		return;
	}

	ShortTermMemory.Add(Event);
	if (!bCompressionInFlight && ShortTermMemory.Num() >= ShortTermMemoryCap)
	{
		RequestCompression();
	}
}

void UNPCMemoryComponent::RequestCompression()
{
	bCompressionInFlight    = true;
	PendingCompressionBatch = MoveTemp(ShortTermMemory);
	ShortTermMemory.Reset();

	FString Joined;
	for (const FNPCMemoryEvent& E : PendingCompressionBatch)
	{
		Joined += E.Content;
		Joined += TEXT("\n");
	}

	UNPCBrainSubsystem* Brain = UNPCBrainSubsystem::Get(this);
	if (!Brain)
	{
		// No brain subsystem available (e.g. headless test, or server not configured).
		// Fall back to a naive concatenation so the events aren't silently lost.
		OnCompressionComplete(Joined);
		return;
	}

	TArray<FNPCMessage> Messages;
	FNPCMessage Sys;
	Sys.Role = ENPCMessageRole::System;
	Sys.Content = TEXT(
		"請將以下一連串事件壓縮成一段簡短摘要（100字以內），"
		"保留對 NPC 重要的資訊，省略不重要的細節。只回覆摘要本文，不要其他文字。");
	Messages.Add(Sys);

	FNPCMessage User;
	User.Role    = ENPCMessageRole::User;
	User.Content = Joined;
	Messages.Add(User);

	FOnInferenceComplete OnComplete;
	OnComplete.BindUObject(this, &UNPCMemoryComponent::OnCompressionComplete);
	FOnInferenceError OnError;
	OnError.BindUObject(this, &UNPCMemoryComponent::OnCompressionError);

	Brain->SendMessages(Messages, OnComplete, OnError);
}

void UNPCMemoryComponent::OnCompressionComplete(const FString& Summary)
{
	if (!LongTermSummary.IsEmpty()) LongTermSummary += TEXT("\n");
	LongTermSummary += Summary;

	PendingCompressionBatch.Reset();
	bCompressionInFlight = false;
}

void UNPCMemoryComponent::OnCompressionError(int32 /*HttpStatusCode*/, const FString& ErrorMsg)
{
	UE_LOG(LogTemp, Warning, TEXT("NPCBrain: memory compression failed, re-queuing batch: %s"), *ErrorMsg);

	// Re-queue at the front so these events get another chance next time the
	// cap is reached, rather than being silently dropped.
	ShortTermMemory.Insert(PendingCompressionBatch, 0);
	PendingCompressionBatch.Reset();
	bCompressionInFlight = false;
}
