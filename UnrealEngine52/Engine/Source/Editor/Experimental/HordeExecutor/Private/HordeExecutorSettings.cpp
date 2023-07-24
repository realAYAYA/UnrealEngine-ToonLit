// Copyright Epic Games, Inc. All Rights Reserved.

#include "HordeExecutorSettings.h"

#include "HAL/Platform.h"

UHordeExecutorSettings::UHordeExecutorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ContentAddressableStorageTarget = TEXT("https://horde.devtools-dev.epicgames.com");

	ExecutionTarget = TEXT("https://horde.devtools-dev.epicgames.com");

	ContentAddressableStorageHeaders.Add("Authorization", "ServiceAccount 0f8056b30bd0df0959be55fc3338159b6f938456d3474aed0087fb965268d079");

	ExecutionHeaders.Add("Authorization", "ServiceAccount 0f8056b30bd0df0959be55fc3338159b6f938456d3474aed0087fb965268d079");
}
