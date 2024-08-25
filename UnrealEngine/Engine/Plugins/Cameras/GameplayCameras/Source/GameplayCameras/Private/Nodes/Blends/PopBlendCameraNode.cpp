// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Blends/PopBlendCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PopBlendCameraNode)

void UPopBlendCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
}

void UPopBlendCameraNode::OnBlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult)
{
	const FCameraNodeRunResult& ChildResult(Params.ChildResult);
	FCameraNodeRunResult& BlendedResult(OutResult.BlendedResult);
	
	BlendedResult.CameraPose.OverrideChanged(ChildResult.CameraPose);
	if (ChildResult.bIsCameraCut || Params.ChildParams.bIsFirstFrame)
	{
		BlendedResult.bIsCameraCut = true;
	}

	OutResult.bIsBlendFull = true;
	OutResult.bIsBlendFinished = true;
}

