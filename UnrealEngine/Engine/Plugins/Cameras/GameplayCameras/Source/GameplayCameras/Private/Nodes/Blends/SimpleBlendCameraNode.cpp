// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Blends/SimpleBlendCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimpleBlendCameraNode)

void USimpleBlendCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	FSimpleBlendCameraNodeRunResult FactorResult;
	OnComputeBlendFactor(Params, FactorResult);
	BlendFactor = FactorResult.BlendFactor;
}

void USimpleBlendCameraNode::OnBlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult)
{
	const FCameraNodeRunResult& ChildResult(Params.ChildResult);
	FCameraNodeRunResult& BlendedResult(OutResult.BlendedResult);

	BlendedResult.CameraPose.LerpChanged(
			ChildResult.CameraPose, 
			BlendFactor,
			ChildResult.CameraPose.GetChangedFlags(), // or is it BlendedResult flags?
			false,
			BlendedResult.CameraPose.GetChangedFlags());

	// If we have even a fraction of a camera cut, we need to make the
	// whole result into a camera cut.
	if (BlendFactor > 0.f && ChildResult.bIsCameraCut)
	{
		BlendedResult.bIsCameraCut = true;
	}

	OutResult.bIsBlendFull = BlendFactor >= 1.f;
	OutResult.bIsBlendFinished = bIsBlendFinished;
}

void USimpleFixedTimeBlendCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	CurrentTime += Params.DeltaTime;
	if (CurrentTime >= BlendTime)
	{
		CurrentTime = BlendTime;
		SetBlendFinished();
	}

	Super::OnRun(Params, OutResult);
}

