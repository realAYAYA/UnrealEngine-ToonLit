// Copyright Epic Games, Inc. All Rights Reserved.

#include "DesktopPlatformModule.h"
#include "DesktopPlatformPrivate.h"

IMPLEMENT_MODULE( FDesktopPlatformModule, DesktopPlatform );
DEFINE_LOG_CATEGORY(LogDesktopPlatform);

void FDesktopPlatformModule::StartupModule()
{
	DesktopPlatform = new FDesktopPlatform();

	FPlatformMisc::SetEnvironmentVar(TEXT("UE_DesktopUnrealProcess"), TEXT("1"));
}

void FDesktopPlatformModule::ShutdownModule()
{
	FPlatformMisc::SetEnvironmentVar(TEXT("UE_DesktopUnrealProcess"), TEXT("0"));

	if (DesktopPlatform != NULL)
	{
		delete DesktopPlatform;
		DesktopPlatform = NULL;
	}
}
