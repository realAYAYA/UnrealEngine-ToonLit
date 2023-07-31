// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationTool.h"

#include "Modules/ModuleManager.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UnrealVirtualizationToolApp.h"

IMPLEMENT_APPLICATION(UnrealVirtualizationTool, "UnrealVirtualizationTool");

DEFINE_LOG_CATEGORY(LogVirtualizationTool);

bool UnrealVirtualizationToolMain(int32 ArgC, TCHAR* ArgV[])
{
	using namespace UE::Virtualization;

	GEngineLoop.PreInit(ArgC, ArgV);
	check(GConfig && GConfig->IsReadyForUse());

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

	// Even though we are exiting anyway we need to request an engine exit in order to get a clean shutdown
	RequestEngineExit(TEXT("The process has finished"));

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return bRanSuccessfully;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	return UnrealVirtualizationToolMain(ArgC, ArgV) ? 0 : 1;
}