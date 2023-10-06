// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "RequiredProgramMainCPPInclude.h"
#include "PakFileUtilities.h"
#include "IPlatformFilePak.h"

IMPLEMENT_APPLICATION(UnrealPak, "UnrealPak");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	double StartTime = FPlatformTime::Seconds();

	int32 Result = ExecuteUnrealPak(FCommandLine::Get())? 0 : 1;

	UE_LOG(LogPakFile, Display, TEXT("UnrealPak executed in %f seconds"), FPlatformTime::Seconds() - StartTime);

	if (FParse::Param(FCommandLine::Get(), TEXT("fastexit")))
	{
		FPlatformMisc::RequestExitWithStatus(true, Result);
	}

	GLog->Flush();

	RequestEngineExit(TEXT("UnrealPak Exiting"));

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return Result;
}

