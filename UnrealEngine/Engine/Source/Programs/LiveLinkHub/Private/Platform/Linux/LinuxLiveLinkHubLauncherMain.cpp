// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LiveLinkHubRun.h"
#include "LaunchEngineLoop.h"
#include "UnixCommonStartup.h"

int main(int argc, char *argv[])
{
	return CommonUnixMain(argc, argv, &RunLiveLinkHub, [] { } );
}
