// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestCommon/Initialization.h"
#include "TestCommon/CoreUtilities.h"
#include "TestCommon/CoreUObjectUtilities.h"
#include "TestCommon/EngineUtilities.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DelayedAutoRegister.h"

static IPlatformFile* DefaultPlatformFile;

void InitAllThreadPoolsEditorEx(bool MultiThreaded)
{
#if WITH_EDITOR
	InitEditorThreadPools();
#endif // WITH_EDITOR
	InitAllThreadPools(MultiThreaded);
}

void InitStats()
{
#if STATS
	FThreadStats::StartThread();
#endif // #if STATS

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::StatSystemReady);
}

void UsePlatformFileStubIfRequired()
{
#if WITH_ENGINE && UE_LLT_USE_PLATFORM_FILE_STUB
	if (IPlatformFile* WrapperFile = FPlatformFileManager::Get().GetPlatformFile(TEXT("PlatformFileStub")))
	{
		IPlatformFile* CurrentPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
		WrapperFile->Initialize(CurrentPlatformFile, TEXT(""));
		FPlatformFileManager::Get().SetPlatformFile(*WrapperFile);
	}
#endif // WITH_ENGINE && UE_LLT_USE_PLATFORM_FILE_STUB
}

void SaveDefaultPlatformFile()
{
	DefaultPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
}

void UseDefaultPlatformFile()
{
	FPlatformFileManager::Get().SetPlatformFile(*DefaultPlatformFile);
}

void InitAll(bool bAllowLogging, bool bMultithreaded)
{
	SaveDefaultPlatformFile();
	UsePlatformFileStubIfRequired();
	InitAllThreadPools(bMultithreaded);
#if WITH_ENGINE
	InitAsyncQueues();
#endif // WITH_ENGINE
	InitTaskGraph();
#if WITH_ENGINE
	InitGWarn();
	InitEngine();
#endif // WITH_ENGINE
#if WITH_EDITOR
	InitDerivedDataCache();
	InitSlate();
	InitForWithEditorOnlyData();
	InitEditor();
#endif // WITH_EDITOR
#if WITH_COREUOBJECT
	InitCoreUObject();
#endif
	GIsRunning = true;
}

void CleanupAll()
{
#if WITH_ENGINE
	CleanupEngine();
#endif
#if WITH_COREUOBJECT
	CleanupCoreUObject();
#endif
	CleanupAllThreadPools();
	CleanupTaskGraph();
}