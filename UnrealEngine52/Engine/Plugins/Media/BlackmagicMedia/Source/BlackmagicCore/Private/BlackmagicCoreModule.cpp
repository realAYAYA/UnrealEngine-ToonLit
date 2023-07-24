// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackmagicCoreModule.h"
#include "BlackmagicLib.h"


DEFINE_LOG_CATEGORY(LogBlackmagicCore);

void FBlackmagicCoreModule::StartupModule()
{
	bCanForceBlackmagicUsage = FParse::Param(FCommandLine::Get(), TEXT("forceblackmagicusage"));

	if (CanUseBlackmagicCard())
	{
		bInitialized = BlackmagicDesign::ApiInitialization();
	}
}
void FBlackmagicCoreModule::ShutdownModule()
{
	if (bInitialized)
	{
		bInitialized = false;
		BlackmagicDesign::ApiUninitialization();
	}
}


IMPLEMENT_MODULE(FBlackmagicCoreModule, BlackmagicCore);
