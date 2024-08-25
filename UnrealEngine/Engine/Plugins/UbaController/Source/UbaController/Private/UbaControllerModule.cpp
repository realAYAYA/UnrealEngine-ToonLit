// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaControllerModule.h"

#include "Misc/ConfigCacheIni.h"
#include "UbaJobProcessor.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"


DEFINE_LOG_CATEGORY(LogUbaController);

namespace UbaControllerModule
{
	static constexpr int32 SubFolderCount = 32;
};

FUbaControllerModule::FUbaControllerModule()
	: bSupported(false)
	, bModuleInitialized(false)
	, bControllerInitialized(false)
	, RootWorkingDirectory(FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("UbaControllerWorkingDir")))
	, WorkingDirectory(FPaths::Combine(RootWorkingDirectory, FGuid::NewGuid().ToString(EGuidFormats::Digits)))
	, NextFileID(0)
	, NextTaskID(0)
{
}

FUbaControllerModule::~FUbaControllerModule()
{	
	if (JobDispatcherThread)
	{
		JobDispatcherThread->Stop();
		// Wait until the thread is done
		FPlatformProcess::ConditionalSleep([&](){ return JobDispatcherThread && JobDispatcherThread->IsWorkDone(); },0.1f);
	}

	CleanWorkingDirectory();
}

bool FUbaControllerModule::IsSupported()
{
	if (bControllerInitialized)
	{
		return bSupported;
	}
	
#if PLATFORM_WINDOWS
	bool bEnabled = false;
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoUbaController")))
	{
		GConfig->GetBool(TEXT("UbaController"), TEXT("Enabled"), bEnabled, GEngineIni);
	}

	bSupported = FPlatformProcess::SupportsMultithreading() && bEnabled;
#else
	// Not supported on other platforms.
	bSupported = false;
#endif
	return bSupported;
}

void FUbaControllerModule::CleanWorkingDirectory() const
{
	if (UE::GetMultiprocessId() != 0) // Only director is allowed to clean
	{
		return;
	}

	IFileManager& FileManager = IFileManager::Get();
	
	if (!RootWorkingDirectory.IsEmpty())
	{
		if (!FileManager.DeleteDirectory(*RootWorkingDirectory, false, true))
		{
			UE_LOG(LogUbaController, Log, TEXT("%s => Failed to delete current working Directory => %s"), ANSI_TO_TCHAR(__FUNCTION__), *RootWorkingDirectory);
		}	
	}
}

bool FUbaControllerModule::HasTasksDispatchedOrPending() const
{
	return !PendingRequestedCompilationTasks.IsEmpty() || (JobDispatcherThread.IsValid() && JobDispatcherThread->HasJobsInFlight());
}

FString GetUbaBinariesPath()
{
#if PLATFORM_CPU_ARM_FAMILY
	const TCHAR* BinariesArch = TEXT("arm64");
#else
	const TCHAR* BinariesArch = TEXT("x64");
#endif
	return FPaths::Combine(FPaths::EngineDir(), "Binaries/Win64/UnrealBuildAccelerator", BinariesArch);
}

void FUbaControllerModule::LoadDependencies()
{
	const FString UbaBinariesPath = GetUbaBinariesPath();
	FPlatformProcess::AddDllDirectory(*UbaBinariesPath);
	FPlatformProcess::GetDllHandle(*(FPaths::Combine(UbaBinariesPath, "UbaHost.dll")));
}

void FUbaControllerModule::StartupModule()
{
	check(!bModuleInitialized);

	LoadDependencies();

	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureType(), this);

	bModuleInitialized = true;

	FCoreDelegates::OnEnginePreExit.AddLambda([&]()
	{
		if (bControllerInitialized && JobDispatcherThread)
		{
			JobDispatcherThread->Stop();
			FPlatformProcess::ConditionalSleep([&]() { return JobDispatcherThread && JobDispatcherThread->IsWorkDone(); }, 0.1f);
			JobDispatcherThread = nullptr;
		}
	});
}

void FUbaControllerModule::ShutdownModule()
{
	check(bModuleInitialized);
	
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureType(), this);
	
	if (bControllerInitialized)
	{
		// Stop the jobs thread
		if (JobDispatcherThread)
		{
			JobDispatcherThread->Stop();	
			// Wait until the thread is done
			FPlatformProcess::ConditionalSleep([&](){ return JobDispatcherThread && JobDispatcherThread->IsWorkDone(); },0.1f);
		}

		FTask* Task;
		while (PendingRequestedCompilationTasks.Dequeue(Task))
		{
			FDistributedBuildTaskResult Result;
			Result.ReturnCode = 0;
			Result.bCompleted = false;
			Task->Promise.SetValue(Result);
			delete Task;
		}

		PendingRequestedCompilationTasks.Empty();
	}

	CleanWorkingDirectory();
	bModuleInitialized = false;
	bControllerInitialized = false;
}

void FUbaControllerModule::InitializeController()
{
	// We should never Initialize the controller twice
	if (ensureAlwaysMsgf(!bControllerInitialized, TEXT("Multiple initialization of UBA controller!")))
	{
		CleanWorkingDirectory();

		if (IsSupported())
		{
			IFileManager::Get().MakeDirectory(*WorkingDirectory, true);

			// Pre-create the directories so we don't have to explicitly register them to uba later
			for (int32 It = 0; It != UbaControllerModule::SubFolderCount; ++It)
			{
				IFileManager::Get().MakeDirectory(*FString::Printf(TEXT("%s/%d"), *WorkingDirectory, It));
			}


			JobDispatcherThread = MakeShared<FUbaJobProcessor>(*this);
			JobDispatcherThread->StartThread();
		}

		bControllerInitialized = true;
	}
}

FString FUbaControllerModule::CreateUniqueFilePath()
{
	check(bSupported);
	int32 ID = NextFileID++;
	int32 FolderID = ID % UbaControllerModule::SubFolderCount; // We use sub folders to be nicer to file system (we can end up with 20000 files in one folder otherwise)
	return FString::Printf(TEXT("%s/%d/%d.uba"), *WorkingDirectory, FolderID, ID);
}

TFuture<FDistributedBuildTaskResult> FUbaControllerModule::EnqueueTask(const FTaskCommandData& CommandData)
{
	check(bSupported);

	TPromise<FDistributedBuildTaskResult> Promise;
	TFuture<FDistributedBuildTaskResult> Future = Promise.GetFuture();

	// Enqueue the new task
	FTask* Task = new FTask(NextTaskID++, CommandData, MoveTemp(Promise));
	{
		PendingRequestedCompilationTasks.Enqueue(Task);
	}

	JobDispatcherThread->HandleTaskQueueUpdated(CommandData.InputFileName);

	return MoveTemp(Future);
}

void FUbaControllerModule::ReportJobProcessed(const FTaskResponse& InTaskResponse, FTask* CompileTask)
{
	if (CompileTask)
	{
		FDistributedBuildTaskResult Result;
		Result.ReturnCode = InTaskResponse.ReturnCode;
		Result.bCompleted = true;
		CompileTask->Promise.SetValue(Result);
		delete CompileTask;
	}
}

UBACONTROLLER_API FUbaControllerModule& FUbaControllerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUbaControllerModule>(TEXT("UbaController"));
}

IMPLEMENT_MODULE(FUbaControllerModule, UbaController);
