// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionNode.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionSubsystem.h"
#include "StateTreeLinker.h"

bool FAvaTransitionNode::LinkNode(FStateTreeLinker& InLinker)
{
	InLinker.LinkExternalData(TransitionContextHandle);
	InLinker.LinkExternalData(TransitionSubsystemHandle);
	return true;
}
