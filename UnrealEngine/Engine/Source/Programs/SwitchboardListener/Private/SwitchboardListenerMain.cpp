// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchboardListener.h"
#include "SwitchboardListenerApp.h"
#include "SwitchboardListenerVersion.h"

#include "HAL/ExceptionHandling.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "RequiredProgramMainCPPInclude.h"

IMPLEMENT_APPLICATION(SwitchboardListener, "SwitchboardListener");
DEFINE_LOG_CATEGORY(LogSwitchboard);


int32 InitEngine(const TCHAR* InCommandLine)
{
	const int32 InitResult = GEngineLoop.PreInit(InCommandLine);
	if (InitResult != 0)
	{
		return InitResult;
	}

	ProcessNewlyLoadedUObjects();
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Load internal Concert plugins in the pre-default phase
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	// Load Concert Sync plugins in default phase
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default);

	return 0;
}

bool InitSocketSystem()
{
	EModuleLoadResult LoadResult;
	FModuleManager::Get().LoadModuleWithFailureReason(TEXT("Sockets"), LoadResult);

	FIPv4Endpoint::Initialize();

	return LoadResult == EModuleLoadResult::Success;
}


// There's a subtle potential race condition with the parent PID being recycled
// after the parent listener has actually exited. We open a handle first, then
// ensure we have the right process by enumerating processes to check that it's
// actually our parent process. At that point, the open handle is unambiguous.
FProcHandle CheckRedeploy(const FSwitchboardCommandLineOptions& Options)
{
	if (!Options.RedeployFromPid.IsSet())
	{
		return FProcHandle();
	}

	const uint32 ExpectedParentPid = Options.RedeployFromPid.GetValue();
	const FProcHandle ExpectedParentProc = FPlatformProcess::OpenProcess(ExpectedParentPid);
	if (!ExpectedParentProc.IsValid())
	{
		UE_LOG(LogSwitchboard, Error, TEXT("Couldn't get handle to redeploy parent process; you'll need to remove the old executable manually"));
		return FProcHandle();
	}

	const uint32 CurrentPid = FPlatformProcess::GetCurrentProcessId();
	FPlatformProcess::FProcEnumerator ProcessEnumerator;
	while (ProcessEnumerator.MoveNext())
	{
		FPlatformProcess::FProcEnumInfo CurrentProcInfo = ProcessEnumerator.GetCurrent();
		const bool bIsCurrentProc = CurrentProcInfo.GetPID() == CurrentPid;
		const bool bHasExpectedParent = CurrentProcInfo.GetParentPID() == ExpectedParentPid;
		if (bIsCurrentProc)
		{
			if (bHasExpectedParent)
			{
				return ExpectedParentProc;
			}
			else
			{
				UE_LOG(LogSwitchboard, Error, TEXT("Redeploy expected parent PID of %u, but found %u; you'll need to remove the old executable manually"),
					ExpectedParentPid, CurrentProcInfo.GetParentPID());

				return FProcHandle();
			}
		}
	}

	// Shouldn't be possible.
	check(false);
	UE_LOG(LogSwitchboard, Error, TEXT("Failed to enumerate our own PID!"));

	return FProcHandle();
}

// Wait for the old listener parent process to exit and delete its executable.
bool HandleRedeploy(FProcHandle& RedeployParentProc, uint32 RedeployParentPid)
{
#if PLATFORM_WINDOWS
	// NOTE: FPlatformProcess::ExecutablePath() is stale if moved while running!
	const FString CurrentExePath = FPlatformProcess::GetApplicationName(FPlatformProcess::GetCurrentProcessId());

	// Otherwise, the window title remains the temporary (pre-rename) filename.
	::SetConsoleTitle(*CurrentExePath);
#endif

	const FString& OldLauncherPath = FPlatformProcess::GetApplicationName(RedeployParentPid);

	// Get handle to existing IPC semaphore.
	const FString IpcSemaphoreName = FSwitchboardListener::GetIpcSemaphoreName(RedeployParentPid);
	FPlatformProcess::FSemaphore* IpcSemaphore = FPlatformProcess::NewInterprocessSynchObject(IpcSemaphoreName, false);
	ON_SCOPE_EXIT
	{
		if (IpcSemaphore)
		{
			FPlatformProcess::DeleteInterprocessSynchObject(IpcSemaphore);
		}
	};

	if (IpcSemaphore == nullptr)
	{
		UE_LOG(LogSwitchboard, Fatal, TEXT("Error opening redeploy IPC semaphore"));
		return false;
	}

	// This release signals the old parent launcher that we're initialized and it can shut down.
	IpcSemaphore->Unlock();

	if (FPlatformProcess::IsProcRunning(RedeployParentProc))
	{
		UE_LOG(LogSwitchboard, Display, TEXT("Waiting for previous listener to shut down..."));
		int8 SecondsToWait = 5;
		bool bStillRunning = true;
		do
		{
			FPlatformProcess::Sleep(1.0f);
			--SecondsToWait;
			bStillRunning = FPlatformProcess::IsProcRunning(RedeployParentProc);
		} while (bStillRunning && SecondsToWait > 0);

		if (bStillRunning)
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("Forcibly terminating previous listener"));
			if (RedeployParentProc.IsValid())
			{
				FPlatformProcess::TerminateProc(RedeployParentProc);
				FPlatformProcess::CloseProc(RedeployParentProc);
			}
			bStillRunning = FPlatformProcess::IsApplicationRunning(RedeployParentPid);
		}

		if (bStillRunning)
		{
			UE_LOG(LogSwitchboard, Error,
				TEXT("Unable to shut down previous listener at %s; you'll need to remove the old executable manually"),
				*OldLauncherPath);
			return false;
		}
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DeleteFile(*OldLauncherPath))
	{
		UE_LOG(LogSwitchboard, Error,
			TEXT("Unable to remove previous listener at %s; you may need to remove the old executable manually"),
			*OldLauncherPath);
		return false;
	}

	return true;
}

int32 RunSwitchboardListener(int32 ArgC, TCHAR* ArgV[])
{
	const FString CommandLine = FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr);
	FCommandLine::Set(*CommandLine);

	const FSwitchboardCommandLineOptions Options = FSwitchboardCommandLineOptions::FromString(*CommandLine);

	if (Options.OutputVersion)
	{
		printf("SwitchboardListener %u.%u.%u", SBLISTENER_VERSION_MAJOR, SBLISTENER_VERSION_MINOR, SBLISTENER_VERSION_PATCH);
		RequestEngineExit(TEXT("Output -version"));
		return 0;
	}

#if PLATFORM_WINDOWS
	// If we were launched detached, detect and spawn a new console.
	// TODO: Command line switch to suppress this behavior? Or explicitly opt-in for redeploy?
	{
		bool bIsStdoutAttachedToConsole = false;
		HANDLE StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (StdoutHandle != INVALID_HANDLE_VALUE)
		{
			if (GetFileType(StdoutHandle) == FILE_TYPE_CHAR)
			{
				bIsStdoutAttachedToConsole = true;
			}
		}

		if (!bIsStdoutAttachedToConsole)
		{
			AllocConsole();

			FILE* OutIgnoredNewStreamPtr = nullptr;
			freopen_s(&OutIgnoredNewStreamPtr, "CONIN$", "r", stdin);
			freopen_s(&OutIgnoredNewStreamPtr, "CONOUT$", "w", stdout);
			freopen_s(&OutIgnoredNewStreamPtr, "CONOUT$", "w", stderr);
		}
	}
#endif

	const int32 InitResult = InitEngine(*CommandLine);

	UE_LOG(LogSwitchboard, Display, TEXT("SwitchboardListener %u.%u.%u"), SBLISTENER_VERSION_MAJOR, SBLISTENER_VERSION_MINOR, SBLISTENER_VERSION_PATCH);

	if (InitResult != 0)
	{
		UE_LOG(LogSwitchboard, Fatal, TEXT("Could not initialize engine, Error code: %d"), InitResult);
		RequestEngineExit(TEXT("Engine init failure"));
		return InitResult;
	}

	UE_LOG(LogSwitchboard, Display, TEXT("Successfully initialized engine."));

	if (!InitSocketSystem())
	{
		UE_LOG(LogSwitchboard, Fatal, TEXT("Could not initialize socket system!"));
		RequestEngineExit(TEXT("Socket init failure"));
		return 1;
	}

	UE_LOG(LogSwitchboard, Display, TEXT("Successfully initialized socket system."));

	FProcHandle RedeployParentProc = CheckRedeploy(Options);
	if (RedeployParentProc.IsValid())
	{
		UE_LOG(LogSwitchboard, Display, TEXT("Performing redeploy"));
		HandleRedeploy(RedeployParentProc, Options.RedeployFromPid.GetValue());
	}

#if PLATFORM_WINDOWS
	if (Options.MinimizeOnLaunch)
	{
		ShowWindow(GetConsoleWindow(), SW_MINIMIZE);
	}
#endif

	FSwitchboardListener Listener(Options);

	if (!Listener.Init())
	{
		RequestEngineExit(TEXT("FSwitchboardListener init failure"));
		return 1;
	}

	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / 30.0f;

	bool bListenerIsRunning = true;

	while (bListenerIsRunning)
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;

		if (DeltaTime > 0.1)
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("Hitch detected; %.3f seconds since prior tick"), DeltaTime);
		}

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		// Pump & Tick objects
		FTSTicker::GetCoreTicker().Tick(DeltaTime);

		bListenerIsRunning = Listener.Tick();

		GFrameCounter++;
		FStats::AdvanceFrame(false);
		GLog->FlushThreadedLogs();

		// Run garbage collection for the UObjects for the rest of the frame or at least to 2 ms
		IncrementalPurgeGarbage(true, FMath::Max<float>(0.002f, IdealFrameTime - (FPlatformTime::Seconds() - CurrentTime)));

		// Throttle main thread main fps by sleeping if we still have time
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - CurrentTime)));
	}

	// Cleanup. See also: FEngineLoop::AppPreExit and FEngineLoop::AppExit
	UE_LOG(LogExit, Log, TEXT("Preparing to exit."));
	FCoreDelegates::OnPreExit.Broadcast();
	FCoreDelegates::OnExit.Broadcast();

	if (GThreadPool != nullptr)
	{
		GThreadPool->Destroy();
	}
	if (GBackgroundPriorityThreadPool != nullptr)
	{
		GBackgroundPriorityThreadPool->Destroy();
	}
	if (GIOThreadPool != nullptr)
	{
		GIOThreadPool->Destroy();
	}

	FTaskGraphInterface::Shutdown();
	FPlatformMisc::PlatformTearDown();

	if (GLog)
	{
		GLog->TearDown();
	}

	FTextLocalizationManager::TearDown();
	FInternationalization::TearDown();

	FTraceAuxiliary::Shutdown();

	return 0;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	int32 ExitCode;
	if (FPlatformMisc::IsDebuggerPresent())
	{
		ExitCode = RunSwitchboardListener(ArgC, ArgV);
	}
	else
	{
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif
		{
			// SetCrashHandler(nullptr) sets up default behavior for Linux and Mac interfacing with CrashReportClient
			FPlatformMisc::SetCrashHandler(nullptr);
			GIsGuarded = true;
			ExitCode = RunSwitchboardListener(ArgC, ArgV);
			GIsGuarded = false;
		}
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except (ReportCrash(GetExceptionInformation()))
		{
			if (GError)
			{
				GError->HandleError();
			}

			// RequestExit(bForce==True) terminates this process with a specific exit code and does not return.
			FPlatformMisc::RequestExit(true);
			ExitCode = 1; // Suppress compiler warning.
		}
#endif
	}

	return ExitCode;
}
