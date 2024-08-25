// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/Blends/SimpleBlendCameraNode.h"

#include "SmoothBlendCameraNode.generated.h"

/**
 * The smoothstep type.
 */
UENUM()
enum class ESmoothCameraBlendType
{
	SmoothStep,
	SmootherStep
};

/**
 * A blend camera mode that implements the smoothstep and smoothersteps algorithms.
 */
UCLASS(MinimalAPI)
class USmoothBlendCameraNode : public USimpleFixedTimeBlendCameraNode
{
	GENERATED_BODY()

protected:

	virtual void OnComputeBlendFactor(const FCameraNodeRunParams& Params, FSimpleBlendCameraNodeRunResult& OutResult) override;
	
public:

	/** The type of algorithm to use. */
	UPROPERTY(EditAnywhere, Category=Blending)
	ESmoothCameraBlendType BlendType;
};

