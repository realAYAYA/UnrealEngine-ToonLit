// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "HordeExecutorSettings.generated.h"

UCLASS(config = EditorSettings)
class UHordeExecutorSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** The Horde server content addressable storage address. */
	UPROPERTY(Config, EditAnywhere, Category = "Horde", meta = (DisplayName = "Content Addressable Storage Target", ConfigRestartRequired = true))
	FString ContentAddressableStorageTarget;

	/** The Horde server execution address. */
	UPROPERTY(Config, EditAnywhere, Category = "Horde", meta = (DisplayName = "Execution Target", ConfigRestartRequired = true))
	FString ExecutionTarget;

	/** Extra headers required for content addressable storage requests. */
	UPROPERTY(Config, EditAnywhere, Category = "Horde", meta = (DisplayName = "Content Addressable Storage Headers", ConfigRestartRequired = true))
	TMap<FString, FString> ContentAddressableStorageHeaders;

	/** Extra headers required for execution requests. */
	UPROPERTY(Config, EditAnywhere, Category = "Horde", meta = (DisplayName = "Execution Headers", ConfigRestartRequired = true))
	TMap<FString, FString> ExecutionHeaders;
};
