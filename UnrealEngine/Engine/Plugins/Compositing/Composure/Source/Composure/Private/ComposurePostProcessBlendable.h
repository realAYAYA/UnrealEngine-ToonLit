// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlendableInterface.h"
#include "ComposurePostProcessBlendable.generated.h"


/**
 * Private blendable interface for  UComposurePostProcessPass.
 */
UCLASS(NotBlueprintType)
class UComposurePostProcessBlendable
	: public UObject
	, public IBlendableInterface
{
	GENERATED_UCLASS_BODY()

public:

	// Begins IBlendableInterface
	void OverrideBlendableSettings(class FSceneView& View, float Weight) const override;
	// Ends IBlendableInterface

private:
	// Current player camera manager the target is bind on.
	UPROPERTY(Transient)
	TObjectPtr<class UComposurePostProcessPass> Target;
	
	friend class UComposurePostProcessPass;
};
