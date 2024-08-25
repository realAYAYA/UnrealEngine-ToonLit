// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "UObject/ObjectPtr.h"
#include "AvaTransitionPreviewLevelState.generated.h"

class ULevelStreaming;

USTRUCT()
struct FAvaTransitionPreviewLevelState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULevelStreaming> LevelStreaming;

	UPROPERTY()
	bool bEnableOverrideTransitionLayer = false;

	UPROPERTY()
	FAvaTagHandle OverrideTransitionLayer;

	/** Indication of whether this Level should be unloaded after finishing transition */
	UPROPERTY()
	bool bShouldUnload = false;
};
