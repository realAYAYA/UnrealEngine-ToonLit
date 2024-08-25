// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaJobProcessor.h"

#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/ConfigCacheIni.h"
#include "UbaHordeAgentManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "UbaControllerModule.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

#include "Misc/Paths.h"
#include "Misc/ScopeRWLock.h"

namespace UbaJobProcessorOptions
{
	static float SleepTimeBetweenActions = 0.01f;
	static FAutoConsoleVariableRef CVarSleepTimeBetweenActions(
        TEXT("r.UbaController.SleepTimeBetweenActions"),
        SleepTimeBetweenActions,
        TEXT("How much time the job processor thread should sleep between actions .\n"));

	static float MaxTimeWithoutTasks = 100.0f;
	static FAutoConsoleVariableRef CVarMaxTimeWithoutTasks(
        TEXT("r.UbaController.MaxTimeWithoutTasks"),
        MaxTimeWithoutTasks,
        TEXT("Time to wait (in seconds) before stop processing attempts if we don't have any pending task.\n"));

	static bool bAutoLaunchVisualizer = false;
	static FAutoConsoleVariableRef CVarAutoLaunchVisualizer(
		TEXT("r.UbaController.AutoLaunchVisualizer"),
		bAutoLaunchVisualizer,
		TEXT("If true, UBA visualizer will be launched automatically\n"));

	static FString TraceFilename;
	static FAutoConsoleVariableRef CVarTraceFilename(
		TEXT("r.UbaController.TraceFilename"),
		TraceFilename,
		TEXT("The name of the trace file that uba outputs after a session"));

	static bool bAllowProcessReuse = true;
	static FAutoConsoleVariableRef CVarAllowProcessReuse(
		TEXT("r.UbaController.AllowProcessReuse"),
		bAllowProcessReuse,
		TEXT("If true, remote process is allowed to fetch new processes from the queue (this requires the remote processes to have UbaRequestNextProcess implemented)\n"));

	static bool bDetailedTrace = false;
	static FAutoConsoleVariableRef CVarDetailedTrace(
		TEXT("r.UbaController.DetailedTrace"),
		bDetailedTrace,
		TEXT("If true, a UBA will output detailed trace\n"));

	static bool bShowUbaLog = false;
	static FAutoConsoleVariableRef CVarShowUbaLog(
		TEXT("r.UbaController.ShowUbaLog"),
		bShowUbaLog,
		TEXT("If true, UBA log entries will be visible in the log\n"));

	static bool bProcessLogEnabled = false;
	static FAutoConsoleVariableRef CVarProcessLogEnabled(
		TEXT("r.UbaController.ProcessLogEnabled"),
		bProcessLogEnabled,
		TEXT("If true, each detoured process will write a log file. Note this is only useful if UBA is compiled in debug\n"));

	FString ReplaceEnvironmentVariablesInPath(const FString& ExtraFilePartialPath) // Duplicated code with FAST build.. put it somewhere else?
	{
		FString ParsedPath;

		// Fast build cannot read environmental variables easily
		// Is better to resolve them here
		if (ExtraFilePartialPath.Contains(TEXT("%")))
		{
			TArray<FString> PathSections;
			ExtraFilePartialPath.ParseIntoArray(PathSections, TEXT("/"));

			for (FString& Section : PathSections)
			{
				if (Section.Contains(TEXT("%")))
				{
					Section.RemoveFromStart(TEXT("%"));
					Section.RemoveFromEnd(TEXT("%"));
					Section = FPlatformMisc::GetEnvironmentVariable(*Section);
				}
			}

			for (FString& Section : PathSections)
			{
				ParsedPath /= Section;
			}

			FPaths::NormalizeDirectoryName(ParsedPath);
		}

		if (ParsedPath.IsEmpty())
		{
			ParsedPath = ExtraFilePartialPath;
		}

		return ParsedPath;
	}
}

FUbaJobProcessor::FUbaJobProcessor(
	FUbaControllerModule& InControllerModule) :

	Thread(nullptr),
	ControllerModule(InControllerModule),
	bForceStop(false),
	LastTimeCheckedForTasks(0),
	bShouldProcessJobs(false),
	bIsWorkDone(false),
	LogWriter([]() {}, []() {}, [](uba::LogEntryType type, const wchar_t* str, uba::u32 strlen)
	{
			switch (type)
			{
			case uba::LogEntryType_Error:
				UE_LOG(LogUbaController, Error, TEXT("%s"), str);
				break;
			case uba::LogEntryType_Warning:
				UE_LOG(LogUbaController, Warning, TEXT("%s"), str);
				break;
			default:
				if (UbaJobProcessorOptions::bShowUbaLog)
				{
					UE_LOG(LogUbaController, Display, TEXT("%s"), str);
				}
				break;
			}
	})
{
	Uba_SetCustomAssertHandler([](const uba::tchar* text)
		{
			checkf(false, TEXT("%s"), text);
		});

	if (!GConfig->GetInt(TEXT("UbaController"), TEXT("MaxLocalParallelJobs"), MaxLocalParallelJobs, GEngineIni))
	{
		MaxLocalParallelJobs = -1;
	}

	if (MaxLocalParallelJobs == -1)
	{
		MaxLocalParallelJobs = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	}
}

FUbaJobProcessor::~FUbaJobProcessor()
{
	delete Thread;
}

void FUbaJobProcessor::CalculateKnownInputs()
{
	// TODO: This is ShaderCompileWorker specific and this code is designed to handle all kinds of distributed workload.
	// Instead this information should be provided from the outside


	if (KnownInputsCount) // In order to improve startup we provide some of the input we know will be loaded by ShaderCompileWorker.exe
	{
		return;
	}

	auto AddKnownInput = [&](const FString& file)
		{
			auto& fileData = file.GetCharArray();
			auto num = KnownInputsBuffer.Num();
			KnownInputsBuffer.SetNum(num + fileData.Num());
			memcpy(KnownInputsBuffer.GetData() + num, fileData.GetData(), fileData.Num() * sizeof(uba::tchar));
			++KnownInputsCount;
		};

	// Get the binaries
	TArray<FString> KnownFileNames;
	FString BinDir = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64")); // TODO: Need to support other targets than Win64
	IFileManager::Get().FindFilesRecursive(KnownFileNames, *BinDir, TEXT("ShaderCompileWorker*.*"), true, false);
	for (const FString& file : KnownFileNames)
	{
		if (!file.EndsWith(TEXT(".pdb")))
		{
			AddKnownInput(file);
		}
	}

	// Get the compiler dependencies for all platforms)
	ITargetPlatformManagerModule* TargetPlatformManager = GetTargetPlatformManager();
	for (ITargetPlatform* TargetPlatform : GetTargetPlatformManager()->GetTargetPlatforms())
	{
		KnownFileNames.Empty();
		TargetPlatform->GetShaderCompilerDependencies(KnownFileNames);

		for (const FString& ExtraFilePartialPath : KnownFileNames)
		{
			if (!ExtraFilePartialPath.Contains(TEXT("*"))) // Seems like there are some *.x paths in there.. TODO: Do a find files
			{
				AddKnownInput(UbaJobProcessorOptions::ReplaceEnvironmentVariablesInPath(ExtraFilePartialPath));
			}
		}
	}

	// Get all the config files
	for (const FString& ConfigDir : FPaths::GetExtensionDirs(FPaths::EngineDir(), TEXT("Config")))
	{
		KnownFileNames.Empty();
		IFileManager::Get().FindFilesRecursive(KnownFileNames, *ConfigDir, TEXT("*.ini"), true, false);
		for (const FString& file : KnownFileNames)
		{
			AddKnownInput(file);
		}
	}

	KnownInputsBuffer.Add(0);
}

void FUbaJobProcessor::RunTaskWithUba(FTask* Task)
{
	FTaskCommandData& Data = Task->CommandData;
	SessionServer_RegisterNewFile(UbaSessionServer, *Data.InputFileName);

	FString InputFileName = FPaths::GetCleanFilename(Data.InputFileName);
	FString OutputFileName = FPaths::GetCleanFilename(Data.OutputFileName);
	FString Parameters = FString::Printf(TEXT("\"%s/\" %d 0 \"%s\" \"%s\" %s "), *Data.WorkingDirectory, Data.DispatcherPID, *InputFileName, *OutputFileName, *Data.ExtraCommandArgs);
	FString AppDir = FPaths::GetPath(Data.Command);

	uba::ProcessStartInfo ProcessInfo;
	ProcessInfo.application = *Data.Command;
	ProcessInfo.arguments = *Parameters;
	ProcessInfo.description = *InputFileName;
	ProcessInfo.workingDir = *AppDir;
	ProcessInfo.writeOutputFilesOnFail = true;

	if (UbaJobProcessorOptions::bProcessLogEnabled)
	{
		ProcessInfo.logFile = *InputFileName;
	}
	
	struct ExitedInfo
	{
		FUbaJobProcessor* Processor;
		FString InputFile;
		FString OutputFile;
		FTask* Task;
	};

	auto Info = new ExitedInfo;
	Info->Processor = this;
	Info->InputFile = Data.InputFileName;
	Info->OutputFile = Data.OutputFileName;
	Info->Task = Task;

	ProcessInfo.userData = Info;
	ProcessInfo.exitedFunc = [](void* userData, const uba::ProcessHandle& ph)
		{
			uint32 logLineIndex = 0;
			while (const uba::tchar* line = ProcessHandle_GetLogLine(&ph, logLineIndex++))
			{
				UE_LOG(LogUbaController, Display, TEXT("%s"), line);
			}

			if (auto Info = (ExitedInfo*)userData) // It can be null if custom message has already handled all of them
			{
				IFileManager::Get().Delete(*Info->InputFile);
				SessionServer_RegisterDeleteFile(Info->Processor->UbaSessionServer, *Info->InputFile);
				Info->Processor->HandleUbaJobFinished(Info->Task);

				Storage_DeleteFile(Info->Processor->UbaStorageServer, *Info->InputFile);
				Storage_DeleteFile(Info->Processor->UbaStorageServer, *Info->OutputFile);

				delete Info;
			}
		};


	Scheduler_EnqueueProcess(UbaScheduler, ProcessInfo, 1.0f, KnownInputsBuffer.GetData(), KnownInputsBuffer.Num()*sizeof(uba::tchar), KnownInputsCount);
}

void FUbaJobProcessor::StartUba()
{
	checkf(UbaServer == nullptr, TEXT("FUbaJobProcessor::StartUba() was called twice before FUbaJobProcessor::ShutDownUba()"));

	UbaServer = CreateServer(LogWriter);

	FString RootDir = FString::Printf(TEXT("%s/%s/%u"), FPlatformProcess::UserTempDir(), TEXT("UbaControllerStorageDir"), UE::GetMultiprocessId());
	IFileManager::Get().MakeDirectory(*RootDir, true);

	uba::u64 casCapacityBytes = 32llu * 1024 * 1024 * 1024;
	UbaStorageServer = CreateStorageServer(*UbaServer, *RootDir, casCapacityBytes, true, LogWriter);

	uba::SessionServerCreateInfo info(*UbaStorageServer, *UbaServer, LogWriter);
	info.launchVisualizer = UbaJobProcessorOptions::bAutoLaunchVisualizer;
	info.rootDir = *RootDir;
	info.allowMemoryMaps = false; // Skip using memory maps
	info.remoteLogEnabled = UbaJobProcessorOptions::bProcessLogEnabled;

	info.traceEnabled = true;
	FString TraceOutputFile = UbaJobProcessorOptions::TraceFilename;
	if (!TraceOutputFile.IsEmpty() && UE::GetMultiprocessId())
		TraceOutputFile = FString::Printf(TEXT("%s_%u"), *TraceOutputFile, UE::GetMultiprocessId());
	info.traceOutputFile = *TraceOutputFile;
	info.detailedTrace = UbaJobProcessorOptions::bDetailedTrace;
	FString TraceName = FString::Printf(TEXT("UbaController_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	info.traceName = *TraceName;


	//info.remoteLogEnabled = true;
	UbaSessionServer = CreateSessionServer(info);

	CalculateKnownInputs();

	UbaScheduler = Scheduler_Create(UbaSessionServer, MaxLocalParallelJobs, UbaJobProcessorOptions::bAllowProcessReuse);
	Scheduler_Start(UbaScheduler);

	HandleTaskQueueUpdated(TEXT("")); // Flush tasks into uba scheduler

	if (UE::GetMultiprocessId() == 0)
	{
		Server_StartListen(UbaServer, uba::DefaultPort, nullptr); // Start listen so any helper on the LAN can join in
	}

	HordeAgentManager = MakeUnique<FUbaHordeAgentManager>(ControllerModule.GetWorkingDirectory(), UbaServer);

	UE_LOG(LogUbaController, Display, TEXT("Created UBA storage server: RootDir=%s"), *RootDir);
}

void FUbaJobProcessor::ShutDownUba()
{
	UE_LOG(LogUbaController, Display, TEXT("Shutting down UBA/Horde connection"));

	HordeAgentManager = nullptr;

	if (UbaSessionServer == nullptr)
	{
		return;
	}

	SessionServer_PrintSummary(UbaSessionServer);

	Server_Stop(UbaServer);

	Scheduler_Destroy(UbaScheduler);
	DestroySessionServer(UbaSessionServer);
	DestroyStorageServer(UbaStorageServer);
	DestroyServer(UbaServer);

	UbaScheduler = nullptr;
	UbaSessionServer = nullptr;
	UbaStorageServer = nullptr;
	UbaServer = nullptr;
}

uint32 FUbaJobProcessor::Run()
{
	bIsWorkDone = false;
	
	uint32 LastTimeSinceHadJobs = FPlatformTime::Cycles();	

	while (!bForceStop)
	{
		const float ElapsedSeconds = (FPlatformTime::Cycles() - LastTimeSinceHadJobs) * FPlatformTime::GetSecondsPerCycle();

		uint32 queued = 0;
		uint32 activeLocal = 0;
		uint32 activeRemote = 0;
		uint32 finished = 0;

		FScopeLock lock(&bShouldProcessJobsLock);

		if (UbaScheduler)
		{
			Scheduler_GetStats(UbaScheduler, queued, activeLocal, activeRemote, finished);
		}
		uint32 active = activeLocal + activeRemote;

		// We don't want to hog up Horde resources.
		if (bShouldProcessJobs && ElapsedSeconds > UbaJobProcessorOptions::MaxTimeWithoutTasks && (queued + active) == 0)
		{
			// If we're optimizing job starting, we only want to shutdown UBA if all the processes have terminated
			bShouldProcessJobs = false;
			ShutDownUba();
		}

		// Check if we have new tasks to process
		if ((ControllerModule.HasTasksDispatchedOrPending() || (queued + active) != 0))
		{
			if (!bShouldProcessJobs)
			{
				// We have new tasks. Start processing again
				StartUba();

				bShouldProcessJobs = true;
			}

			LastTimeSinceHadJobs = FPlatformTime::Cycles();
		}

		if (bShouldProcessJobs)
		{
			int32 MaxLocal = FMath::Max(0, int32(MaxLocalParallelJobs / 2) - int32(activeRemote / 10));
			Scheduler_SetMaxLocalProcessors(UbaScheduler, MaxLocal);

			int32 TargetCoreCount = FMath::Max(0, int32(queued + active) - MaxLocal);

			HordeAgentManager->SetTargetCoreCount(TargetCoreCount);
			
			// TODO: Not sure this is a good idea in a cooking scenario where number of queued processes are going up and down
			SessionServer_SetMaxRemoteProcessCount(UbaSessionServer, TargetCoreCount);
		}

		lock.Unlock();

		FPlatformProcess::Sleep(UbaJobProcessorOptions::SleepTimeBetweenActions);
	}

	ShutDownUba();

	bIsWorkDone = true;
	return 0;
}

void FUbaJobProcessor::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("UbaJobProcessor"), 0, TPri_SlightlyBelowNormal, FPlatformAffinity::GetPoolThreadMask());
}

bool FUbaJobProcessor::ProcessOutputFile(FTask* CompileTask)
{
	//TODO: This method is mostly taken from the other Distribution controllers
	// As we get an explicit callback when the process ends, we should be able to simplify this to just check if the file exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager& FileManager = IFileManager::Get();

	constexpr uint64 VersionAndFileSizeSize = sizeof(uint32) + sizeof(uint64);	
	if (ensure(CompileTask && PlatformFile.FileExists(*CompileTask->CommandData.OutputFileName) &&
		FileManager.FileSize(*CompileTask->CommandData.OutputFileName) > VersionAndFileSizeSize))
	{
		const TUniquePtr<FArchive> OutputFilePtr(FileManager.CreateFileReader(*CompileTask->CommandData.OutputFileName, FILEREAD_Silent));
		if (ensure(OutputFilePtr))
		{
			FArchive& OutputFile = *OutputFilePtr;
			int32 OutputVersion;
			OutputFile << OutputVersion; // NOTE (SB): Do not care right now about the version.
			int64 FileSize = 0;
			OutputFile << FileSize;

			// NOTE (SB): Check if we received the full file yet.
			if (ensure(OutputFile.TotalSize() >= FileSize))
			{
				FTaskResponse TaskCompleted;
				TaskCompleted.ID = CompileTask->ID;
				TaskCompleted.ReturnCode = 0;
						
				ControllerModule.ReportJobProcessed(TaskCompleted, CompileTask);
			}
			else
			{
				UE_LOG(LogUbaController, Error, TEXT("Output file size is not correct [%s] | Expected Size [%lld] : => Actual Size : [%lld]"), *CompileTask->CommandData.OutputFileName, OutputFile.TotalSize(), FileSize);
				return false;
			}
		}
		else
		{
			UE_LOG(LogUbaController, Error, TEXT("Failed open for read Output File [%s]"), *CompileTask->CommandData.OutputFileName);
			return false;
		}
	}
	else
	{
		const FString OutputFileName = CompileTask != nullptr ? CompileTask->CommandData.OutputFileName : TEXT("Invalid CompileTask, cannot retrieve name");
		UE_LOG(LogUbaController, Error, TEXT("Output File [%s] is invalid or do not exists"), *OutputFileName);
		return false;
	}

	return true;
}

void FUbaJobProcessor::HandleUbaJobFinished(FTask* CompileTask)
{
	const bool bWasSuccessful = ProcessOutputFile(CompileTask);
	if (!bWasSuccessful)
	{
		// If it failed running locally, lets try Run it locally but outside Uba
		// Signaling a jobs as complete when it wasn't done, will cause a rerun on local worker as fallback
		// because there is not output file for this job

		FTaskResponse TaskCompleted;
		TaskCompleted.ID = CompileTask->ID;
		TaskCompleted.ReturnCode = 0;
		ControllerModule.ReportJobProcessed(TaskCompleted, CompileTask);
	}
}

void FUbaJobProcessor::HandleTaskQueueUpdated(const FString& InputFileName)
{
	FScopeLock lock(&bShouldProcessJobsLock);

	if (!UbaScheduler)
	{
		return;
	}

	while (true)
	{
		FTask* Task = nullptr;
		if (!ControllerModule.PendingRequestedCompilationTasks.Dequeue(Task) || !Task)
			break;
		RunTaskWithUba(Task);
	}
}

bool FUbaJobProcessor::HasJobsInFlight() const
{
	if (!UbaScheduler)
	{
		return false;
	}
	uint32 queued = 0;
	uint32 activeLocal = 0;
	uint32 activeRemote = 0;
	uint32 finished = 0;
	Scheduler_GetStats(UbaScheduler, queued, activeLocal, activeRemote, finished);	
	return (queued + activeLocal + activeRemote) != 0;
}