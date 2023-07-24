// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "CineCameraRigsSettings.generated.h"

class UMaterialInterface;
class UTexture2D;

UCLASS(BlueprintType, Config = CineCameraRigs)
class CINECAMERARIGS_API UCineCameraRigRailSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = CineCameraRigRail)
	TSoftObjectPtr<UMaterialInterface> DefaultSplineMeshMaterial;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = CineCameraRigRail)
	TSoftObjectPtr<UTexture2D> DefaultSplineMeshTexture;
};