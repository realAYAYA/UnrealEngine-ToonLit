// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprints/IDisplayClusterProjectionBlueprintAPI.h"
#include "DisplayClusterProjectionBlueprintAPIImpl.generated.h"


/**
 * Blueprint API interface implementation
 */
UCLASS()
class DISPLAYCLUSTERPROJECTION_API UDisplayClusterProjectionBlueprintAPIImpl
	: public UObject
	, public IDisplayClusterProjectionBlueprintAPI
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Policy: CAMERA
	//////////////////////////////////////////////////////////////////////////////////////////////
	/** Sets active camera component for camera policy */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set camera"), Category = "NDisplayProjection|Camera")
	virtual void CameraPolicySetCamera(const FString& ViewportId, UCameraComponent* NewCamera, float FOVMultiplier = 1.f) override;
};
