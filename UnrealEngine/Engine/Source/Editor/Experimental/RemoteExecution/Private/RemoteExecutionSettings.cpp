// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteExecutionSettings.h"

#include "HAL/Platform.h"

URemoteExecutionSettings::URemoteExecutionSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PreferredRemoteExecutor = TEXT("Bazel");
}
