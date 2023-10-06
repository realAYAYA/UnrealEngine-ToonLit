// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationTool.h"

#include "Modules/ModuleManager.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UnrealVirtualizationToolApp.h"

IMPLEMENT_APPLICATION(UnrealVirtualizationTool, "UnrealVirtualizationTool");

DEFINE_LOG_CATEGORY(LogVirtualizationTool);

int32 UnrealVirtualizationToolMain(int32 ArgC, TCHAR* ArgV[])
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UnrealVirtualizationToolMain);

	using namespace UE::Virtualization;

	// Although standalone tools can set the project path via the cmdline this will not change the ProjectDir being
	// used as standalone tools have a beespoke path in FGenericPlatformMisc::ProjectDir. We can work around this
	// for now by doing our own parsing then using the project dir override feature.
	// This is a band aid while we consider adding better support for project files/directories with stand alone tools.
	if(ArgC >= 2)
	{
		FString Cmd(ArgV[1]);

		if (!Cmd.IsEmpty() && !Cmd.StartsWith(TEXT("-")) && Cmd.EndsWith(FProjectDescriptor::GetExtension()))
		{
			FString ProjectDir = FPaths::GetPath(Cmd);
			ProjectDir = FFileManagerGeneric::DefaultConvertToRelativePath(*ProjectDir);
			
			// The path should end with a trailing slash (see FGenericPlatformMisc::ProjectDir) so we use
			// NormalizeFilename not NormalizeDirectoryName as the latter will remove trailing slashes. We
			// also need to add one if it is missing.
			// We probably should move this path fixup code to 'FPlatformMisc::SetOverrideProjectDir'
			FPaths::NormalizeFilename(ProjectDir);
			if (!ProjectDir.EndsWith(TEXT("/")))
			{
				ProjectDir += TEXT("/");
			}

			FPlatformMisc::SetOverrideProjectDir(ProjectDir);
		}
	}

	GEngineLoop.PreInit(ArgC, ArgV);
	check(GConfig && GConfig->IsReadyForUse());

#if 0
	while (!FPlatformMisc::IsDebuggerPresent())
	{
		FPlatformProcess::SleepNoStats(0.0f);
	}

	PLATFORM_BREAK();
#endif

	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	bool bRanSuccessfully = true;

	FUnrealVirtualizationToolApp App;

	EInitResult Result = App.Initialize();
	if (Result == EInitResult::Success)
	{
		if (!App.Run())
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("UnrealVirtualizationTool ran with errors"));
			bRanSuccessfully = false;
		}
	}	
	else if(Result == EInitResult::Error)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("UnrealVirtualizationTool failed to initialize"));
		bRanSuccessfully = false;
	}

	UE_CLOG(bRanSuccessfully, LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool ran successfully"));

	const uint8 ReturnCode = bRanSuccessfully ? 0 : 1;

	if (FParse::Param(FCommandLine::Get(), TEXT("fastexit")))
	{
		FPlatformMisc::RequestExitWithStatus(true, ReturnCode);
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Shutdown);

		GConfig->DisableFileOperations(); // We don't want to write out any config file changes!

		// Even though we are exiting anyway we need to request an engine exit in order to get a clean shutdown
		RequestEngineExit(TEXT("The process has finished"));

		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	}

	return ReturnCode;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	return UnrealVirtualizationToolMain(ArgC, ArgV);
}