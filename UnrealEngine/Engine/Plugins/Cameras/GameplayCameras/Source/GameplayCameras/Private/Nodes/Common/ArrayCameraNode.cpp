// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/Common/ArrayCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArrayCameraNode)

FCameraNodeChildrenView UArrayCameraNode::OnGetChildren()
{
	return FCameraNodeChildrenView(Children);
}

void UArrayCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	for (TObjectPtr<UCameraNode> Child : Children)
	{
		if (Child)
		{
			Child->Run(Params, OutResult);
		}
	}
}

