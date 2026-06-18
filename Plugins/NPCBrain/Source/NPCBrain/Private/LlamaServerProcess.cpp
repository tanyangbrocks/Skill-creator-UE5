#include "LlamaServerProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

bool FLlamaServerProcess::Launch(const FString& ServerExePath, const FString& ModelPath,
                                  int32 Port, int32 ContextSize, int32 NumThreads)
{
	if (!IFileManager::Get().FileExists(*ServerExePath))
	{
		UE_LOG(LogTemp, Error,
		       TEXT("NPCBrain: llama-server.exe not found: %s"), *ServerExePath);
		return false;
	}
	if (!IFileManager::Get().FileExists(*ModelPath))
	{
		UE_LOG(LogTemp, Error,
		       TEXT("NPCBrain: model file not found: %s"), *ModelPath);
		return false;
	}

	CachedPort = Port;

	// --no-mmap avoids mmap issues on Windows; --host 127.0.0.1 for security
	const FString Args = FString::Printf(
		TEXT("--model \"%s\" --port %d --ctx-size %d --threads %d --host 127.0.0.1 --no-mmap"),
		*ModelPath, Port, ContextSize, NumThreads);

	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	ProcessHandle = FPlatformProcess::CreateProc(
		*ServerExePath,
		*Args,
		/*bLaunchDetached=*/ false,
		/*bLaunchHidden=*/   true,
		/*bLaunchReallyHidden=*/ true,
		nullptr,
		0,
		*FPaths::GetPath(ServerExePath),
		WritePipe,
		ReadPipe
	);

	if (!ProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("NPCBrain: Failed to spawn llama-server.exe"));
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("NPCBrain: llama-server launched (port %d)"), Port);
	return true;
}

void FLlamaServerProcess::Shutdown()
{
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(ProcessHandle, /*bKillTree=*/ true);
		FPlatformProcess::CloseProc(ProcessHandle);
		ProcessHandle.Reset();
		UE_LOG(LogTemp, Log, TEXT("NPCBrain: llama-server stopped"));
	}
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
	}
}

bool FLlamaServerProcess::IsRunning() const
{
	FProcHandle Handle = ProcessHandle;
	return Handle.IsValid() && FPlatformProcess::IsProcRunning(Handle);
}
