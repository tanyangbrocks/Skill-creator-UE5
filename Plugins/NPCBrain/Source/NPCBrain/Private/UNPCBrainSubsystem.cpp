#include "UNPCBrainSubsystem.h"
#include "NPCBrainSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Ticker.h"

UNPCBrainSubsystem* UNPCBrainSubsystem::Get(const UObject* WorldCtx)
{
	if (!WorldCtx) return nullptr;
	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldCtx, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UNPCBrainSubsystem>() : nullptr;
}

bool UNPCBrainSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UNPCBrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNPCBrainSettings* S = UNPCBrainSettings::Get();
	if (!S->bAutoLaunchServer) return;

	if (!ServerProcess.Launch(S->LlamaServerExePath, S->ModelPath,
	                          S->Port, S->ContextSize, S->NumThreads))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("NPCBrain: Server launch failed — check Project Settings > NPC Brain"));
		return;
	}

	InferenceClient.Configure(ServerProcess.GetBaseUrl());
	ReadyPollEndTime = FPlatformTime::Seconds() + S->ReadyTimeoutSeconds;

	// Poll /health every 0.5s until server is ready or timeout
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UNPCBrainSubsystem::PollServerTick), 0.5f);
}

void UNPCBrainSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	ServerProcess.Shutdown();
	Super::Deinitialize();
}

bool UNPCBrainSubsystem::PollServerTick(float /*DeltaTime*/)
{
	if (bServerReady) return false;

	if (FPlatformTime::Seconds() > ReadyPollEndTime)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("NPCBrain: Server did not become ready within timeout. "
		            "Check llama-server output or increase ReadyTimeoutSeconds."));
		TickerHandle.Reset();
		return false;
	}

	if (!ServerProcess.IsRunning())
	{
		UE_LOG(LogTemp, Error, TEXT("NPCBrain: llama-server process exited unexpectedly."));
		TickerHandle.Reset();
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		FHttpModule::Get().CreateRequest();
	Req->SetURL(ServerProcess.GetBaseUrl() + TEXT("/health"));
	Req->SetVerb(TEXT("GET"));
	Req->SetTimeout(1.0f);

	// BindWeakLambda: lambda won't fire if this UObject is GC'd
	Req->OnProcessRequestComplete().BindWeakLambda(this,
		[this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
		{
			if (bServerReady) return;
			if (bOK && Resp && Resp->GetResponseCode() == 200)
			{
				bServerReady = true;
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
				TickerHandle.Reset();
				UE_LOG(LogTemp, Log, TEXT("NPCBrain: Server ready — LLM inference available"));
				OnServerReady.Broadcast();
			}
		});

	Req->ProcessRequest();
	return true; // Keep ticking
}

void UNPCBrainSubsystem::SendMessages(
	const TArray<FNPCMessage>& Messages,
	FOnInferenceComplete       OnComplete,
	FOnInferenceError          OnError,
	float Temperature,
	int32 MaxTokens)
{
	if (!bServerReady)
	{
		OnError.ExecuteIfBound(503, TEXT("NPCBrain: server not ready yet"));
		return;
	}
	InferenceClient.SendMessages(Messages, MoveTemp(OnComplete), MoveTemp(OnError),
	                              Temperature, MaxTokens);
}
