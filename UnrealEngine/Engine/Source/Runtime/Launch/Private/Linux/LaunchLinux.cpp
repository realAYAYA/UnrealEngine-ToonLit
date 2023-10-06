// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LaunchEngineLoop.h"
#include "UnixCommonStartup.h"

extern int32 GuardedMain( const TCHAR* CmdLine );

/**
 * Workaround function to avoid circular dependencies between Launch and CommonUnixStartup modules (see LaunchUnix.cpp)
 */
void LaunchUnix_FEngineLoop_AppExit();

int main(int argc, char *argv[])
{
	return CommonUnixMain(argc, argv, &GuardedMain, &LaunchUnix_FEngineLoop_AppExit);
}
