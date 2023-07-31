// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchTool.h"
#include "UObject/Object.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Interfaces/ToolMode.h"
#include "Misc/OutputDeviceError.h"

using namespace BuildPatchTool;

IMPLEMENT_APPLICATION(BuildPatchTool, "BuildPatchTool");
DEFINE_LOG_CATEGORY(LogBuildPatchTool);

const TCHAR* HandleLegacyCommandline(const TCHAR* CommandLine)
{
	static FString CommandLineString;
	CommandLineString = CommandLine;

#if UE_BUILD_DEBUG
	// Run smoke tests in debug
	CommandLineString += TEXT(" -bForceSmokeTests ");
#endif

	// For BPT, we will interpret -stdout as all log lines go to stdout.
	if (FParse::Param(CommandLine, TEXT("stdout")))
	{
		CommandLineString += TEXT(" -AllowStdOutLogVerbosity ");
	}

	// No longer supported options
	if (CommandLineString.Contains(TEXT("-nochunks")))
	{
		UE_LOG(LogBuildPatchTool, Error, TEXT("NoChunks is no longer a supported mode. Remove this commandline option."));
		return nullptr;
	}

	// Check for legacy tool mode switching, if we don't have a mode and this was not a -help request, add the correct mode
	if (!CommandLineString.Contains(TEXT("-mode=")) && !CommandLineString.Contains(TEXT("-help")))
	{
		if (CommandLineString.Contains(TEXT("-compactify")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-compactify"), TEXT("-mode=compactify"));
		}
		else if (CommandLineString.Contains(TEXT("-dataenumerate")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-dataenumerate"), TEXT("-mode=enumeration"));
		}
		// Patch generation did not have a mode flag, but does have some unique and required params
		else if (CommandLineString.Contains(TEXT("-BuildRoot=")) && CommandLineString.Contains(TEXT("-BuildVersion=")))
		{
			FString NewCommandline(TEXT("-mode=patchgeneration "), CommandLineString.Len());
			NewCommandline += CommandLineString;
			CommandLineString = MoveTemp(NewCommandline);
		}
	}

	return *CommandLineString;
}

EReturnCode RunBuildPatchTool()
{
	// Initialise the UObject module.
	FModuleManager::Get().LoadModule(TEXT("CoreUObject"));
	FCoreDelegates::OnInit.Broadcast();

	// Load the BuildPatchServices Module.
	IBuildPatchServicesModule& BuildPatchServicesModule = FModuleManager::LoadModuleChecked<IBuildPatchServicesModule>(TEXT("BuildPatchServices"));

	// Make sure we have processed UObjects from BPS.
	ProcessNewlyLoadedUObjects();

	// Instantiate and execute the tool.
	TSharedRef<IToolMode> ToolMode = FToolModeFactory::Create(BuildPatchServicesModule);
	return ToolMode->Execute();
}

int32 NumberOfWorkerThreadsDesired()
{
	const int32 MaxThreads = 64;
	const int32 NumberOfCores = FPlatformMisc::NumberOfCores();
	// need to spawn at least one worker thread (see FTaskGraphImplementation)
	return FMath::Max(FMath::Min(NumberOfCores - 1, MaxThreads), 1);
}

void CheckAndReallocThreadPool()
{
	if (FPlatformProcess::SupportsMultithreading())
	{
		const int32 ThreadsSpawned = GThreadPool->GetNumThreads();
		const int32 DesiredThreadCount = NumberOfWorkerThreadsDesired();
		if (ThreadsSpawned < DesiredThreadCount)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("Engine only spawned %d worker threads, bumping up to %d!"), ThreadsSpawned, DesiredThreadCount);
			GThreadPool->Destroy();
			GThreadPool = FQueuedThreadPool::Allocate();
			verify(GThreadPool->Create(DesiredThreadCount, 128 * 1024));
		}
		else
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("Continuing with %d spawned worker threads."), ThreadsSpawned);
		}
	}
}

EReturnCode BuildPatchToolMain(const TCHAR* CommandLine)
{
	// Handle legacy commandlines
	CommandLine = HandleLegacyCommandline(CommandLine);
	if (CommandLine == nullptr)
	{
		return EReturnCode::ArgumentProcessingError;
	}

	// Initialise application
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogBuildPatchTool, Log, TEXT("Executed with commandline: %s"), CommandLine);

	// Check whether as a program, we should bump up the number of threads in GThreadPool.
	CheckAndReallocThreadPool();

	// Run the application
	EReturnCode ReturnCode = RunBuildPatchTool();
	if (ReturnCode != EReturnCode::OK)
	{
		UE_LOG(LogBuildPatchTool, Error, TEXT("Tool exited with: %d"), (int32)ReturnCode);
	}

	// Shutdown
	RequestEngineExit(TEXT("BuildPatchToolMain Exiting"));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return ReturnCode;
}

const TCHAR* ProcessApplicationCommandline(int32 ArgC, TCHAR* ArgV[])
{
	static FString CommandLine = TEXT("-usehyperthreading -UNATTENDED");
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		CommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split(TEXT("="), &ArgName, &ArgValue);
				Argument = FString::Printf(TEXT("%s=\"%s\""), *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		CommandLine += Argument;
	}
	return *CommandLine;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	EReturnCode ReturnCode;
	// Using try&catch is the windows-specific method of interfacing with CrashReportClient
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		// SetCrashHandler(nullptr) sets up default behavior for Linux and Mac interfacing with CrashReportClient
		FPlatformMisc::SetCrashHandler(nullptr);
		GIsGuarded = 1;
		ReturnCode = BuildPatchToolMain(ProcessApplicationCommandline(ArgC, ArgV));
		GIsGuarded = 0;
	}
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except (ReportCrash(GetExceptionInformation()))
	{
		ReturnCode = EReturnCode::Crash;
		GError->HandleError();
	}
#endif
	return static_cast<int32>(ReturnCode);
}
