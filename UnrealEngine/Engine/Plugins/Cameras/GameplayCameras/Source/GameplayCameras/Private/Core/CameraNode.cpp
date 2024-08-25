// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraNode.h"

#include "Core/CameraRuntimeInstantiator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraNode)

void FCameraNodeRunResult::Reset()
{
	CameraPose.Reset();
	bIsCameraCut = false;
	bIsValid = false;
}

FCameraNodeChildrenView UCameraNode::GetChildren()
{
	return OnGetChildren();
}

void UCameraNode::Reset(const FCameraNodeResetParams& Params)
{
	OnReset(Params);
}

void UCameraNode::Run(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	if (bIsEnabled)
	{
		OnRun(Params, OutResult);
	}
}

#if WITH_EDITOR

void UCameraNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	FCameraRuntimeInstantiator::ForwardPropertyChange(this, PropertyChangedEvent);
}

#endif
