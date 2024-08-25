// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/BlendCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlendCameraNode)

void UBlendCameraNode::BlendResults(const FCameraNodeBlendParams& Params, FCameraNodeBlendResult& OutResult)
{
	OnBlendResults(Params, OutResult);
}

