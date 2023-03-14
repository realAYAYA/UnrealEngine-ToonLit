// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyDefines.h"
#include "RigSpacePickerBakeSettings.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIGEDITOR_API FRigSpacePickerBakeSettings
{
	GENERATED_BODY();

	FRigSpacePickerBakeSettings()
	{
		TargetSpace = FRigElementKey();
		StartFrame = 0;
		EndFrame = 100;
		bReduceKeys = false;
		Tolerance = 0.001f;
	}

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FRigElementKey TargetSpace;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Settings")
	FFrameNumber StartFrame;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Settings")
	FFrameNumber EndFrame;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Settings")
	bool bReduceKeys;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Settings")
	float Tolerance;
};
