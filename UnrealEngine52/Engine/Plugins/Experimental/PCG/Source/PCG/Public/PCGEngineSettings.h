// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "PCGEngineSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings)
class PCG_API UPCGEngineSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Specifies the scale of the volume created on PCG graph drag/drop */
	UPROPERTY(EditAnywhere, Config, Category = Workflow)
	FVector VolumeScale = FVector(25, 25, 10);

	/** Whether we want to generate PCG graph/BP with PCG after drag/drop or not */
	UPROPERTY(EditAnywhere, Config, Category = Workflow)
	bool bGenerateOnDrop = true;
};
