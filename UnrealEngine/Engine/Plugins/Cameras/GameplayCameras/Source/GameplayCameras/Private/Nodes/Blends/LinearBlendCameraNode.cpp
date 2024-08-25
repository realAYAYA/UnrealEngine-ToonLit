// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Blends/LinearBlendCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LinearBlendCameraNode)

void ULinearBlendCameraNode::OnComputeBlendFactor(const FCameraNodeRunParams& Params, FSimpleBlendCameraNodeRunResult& OutResult)
{
	OutResult.BlendFactor = FMath::Lerp(0.f, 1.f, GetTimeFactor());
}

