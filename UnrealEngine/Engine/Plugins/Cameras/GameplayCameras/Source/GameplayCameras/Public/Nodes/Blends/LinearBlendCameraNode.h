// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/Blends/SimpleBlendCameraNode.h"

#include "LinearBlendCameraNode.generated.h"

/**
 * Linear blend node.
 */
UCLASS(MinimalAPI)
class ULinearBlendCameraNode : public USimpleFixedTimeBlendCameraNode
{
	GENERATED_BODY()

	virtual void OnComputeBlendFactor(const FCameraNodeRunParams& Params, FSimpleBlendCameraNodeRunResult& OutResult) override;
};

