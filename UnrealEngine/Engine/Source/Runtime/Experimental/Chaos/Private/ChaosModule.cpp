// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"

IMPLEMENT_MODULE(FChaosEngineModule, Chaos);

namespace Chaos
{
	extern CHAOS_API int GSingleThreadedPhysics;
}

void FChaosEngineModule::StartupModule()
{
	if (FModuleManager::Get().ModuleExists(TEXT("GeometryCore")))
	{
		FModuleManager::Get().LoadModule("GeometryCore");
	}

	if(FParse::Param(FCommandLine::Get(), TEXT("SingleThreadedPhysics")))
	{
		Chaos::GSingleThreadedPhysics = 1;
	}
}
