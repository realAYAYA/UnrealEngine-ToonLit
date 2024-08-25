// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Blends/SmoothBlendCameraNode.h"

#include "Math/Interpolation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmoothBlendCameraNode)

void USmoothBlendCameraNode::OnComputeBlendFactor(const FCameraNodeRunParams& Params, FSimpleBlendCameraNodeRunResult& OutResult)
{
	using namespace UE::Cameras;

	const float t = GetTimeFactor();
	switch (BlendType)
	{
		case ESmoothCameraBlendType::SmoothStep:
			OutResult.BlendFactor = SmoothStep(t);
			break;
		case ESmoothCameraBlendType::SmootherStep:
			OutResult.BlendFactor = SmootherStep(t);
			break;
		default:
			OutResult.BlendFactor = 1.f;
			break;
	}
}

