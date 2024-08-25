// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/VCamConnectionStructs.h"
#include "ConnectionContainerDummy.generated.h"

/**
 * This dummy is needed so that the FConnectionTargetSettingsTypeCustomization can access FVCamConnection and use
 * RequiredInterfaces, etc. for filtering suggestions.
 * 
 * The detail customization systems implementation requires a valid parent handle to whatever contains the FVCamConnection,
 * which is FConnectionContainerDummy's task.
 */
USTRUCT()
struct FConnectionContainerDummy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Dummy")
	FVCamConnection Connection;
};