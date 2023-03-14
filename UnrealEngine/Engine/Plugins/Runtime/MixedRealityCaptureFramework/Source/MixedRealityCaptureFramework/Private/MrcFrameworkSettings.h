// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "MrcFrameworkSettings.generated.h"

UCLASS(config=Engine)
class UMrcFrameworkSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(Config)
	FSoftObjectPath DefaulVideoSource;

	UPROPERTY(Config)
	FSoftObjectPath DefaultVideoProcessingMat;

	UPROPERTY(Config)
	FSoftObjectPath DefaultRenderTarget;

	UPROPERTY(Config)
	FSoftObjectPath DefaultDistortionDisplacementMap;

	UPROPERTY(Config)
	FSoftObjectPath DefaulGarbageMatteMesh;

	UPROPERTY(Config)
	FSoftObjectPath DefaulGarbageMatteMaterial;

	UPROPERTY(Config)
	FSoftObjectPath DefaulGarbageMatteTarget;
};
