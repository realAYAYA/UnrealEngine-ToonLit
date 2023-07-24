// Copyright Epic Games, Inc. All Rights Reserved.

#include "RequiredProgramMainCPPInclude.h"

#include "CoreGlobals.h"
#include "DerivedDataCache.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDerivedDataTool, Log, All);

IMPLEMENT_APPLICATION(DerivedDataTool, "DerivedDataTool");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	using namespace UE::DerivedData;

	const FTaskTagScope Scope(ETaskTag::EGameThread);

	if (int32 Ret = GEngineLoop.PreInit(ArgC, ArgV))
	{
		return Ret;
	}

	ON_SCOPE_EXIT
	{
	#if ENABLE_LOW_LEVEL_MEM_TRACKER
		FLowLevelMemTracker::Get().UpdateStatsPerFrame();
	#endif
		RequestEngineExit(TEXT("Exiting"));
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	};

	GetCache();

	return 0;
}
