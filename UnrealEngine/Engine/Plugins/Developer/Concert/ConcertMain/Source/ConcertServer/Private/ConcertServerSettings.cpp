// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerSettings.h"

UConcertServerConfig::UConcertServerConfig()
	: bCleanWorkingDir(false)
	, NumSessionsToKeep(-1)
{
	DefaultVersionInfo.Initialize(false /* bSupportMixedBuildTypes */);
}
