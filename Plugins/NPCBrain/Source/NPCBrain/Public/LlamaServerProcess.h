#pragma once
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

// Manages the llama-server.exe subprocess lifecycle.
class NPCBRAIN_API FLlamaServerProcess
{
public:
	FLlamaServerProcess()  = default;
	~FLlamaServerProcess() { Shutdown(); }

	// Launch llama-server with the given model. Returns false if paths are invalid.
	bool Launch(const FString& ServerExePath, const FString& ModelPath,
	            int32 Port, int32 ContextSize, int32 NumThreads);

	void Shutdown();
	bool IsRunning() const;

	FString GetBaseUrl() const
	{
		return FString::Printf(TEXT("http://127.0.0.1:%d"), CachedPort);
	}

private:
	FProcHandle ProcessHandle;
	void*       ReadPipe  = nullptr;
	void*       WritePipe = nullptr;
	int32       CachedPort = 8080;
};
