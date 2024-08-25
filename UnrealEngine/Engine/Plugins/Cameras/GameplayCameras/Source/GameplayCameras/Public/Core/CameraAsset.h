// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraModeTransition.h"
#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"

#include "CameraAsset.generated.h"

class UCameraDirector;

/**
 * A complete camera asset.
 */
UCLASS(MinimalAPI)
class UCameraAsset : public UObject
{
	GENERATED_BODY()

public:

	/** The camera director to use in this camera. */
	UPROPERTY(Instanced, EditAnywhere, Category=Director)
	TObjectPtr<UCameraDirector> CameraDirector;

	/** A list of default enter transitions for all the camera modes in this asset. */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> EnterTransitions;

	/** A list of default exit transitions for all the camera modes in this asset. */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> ExitTransitions;
};


