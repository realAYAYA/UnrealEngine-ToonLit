// Copyright Epic Games, Inc. All Rights Reserved.

#include "Behaviour/RCBehaviourBlueprintNode.h"

bool URCBehaviourBlueprintNode::Execute_Implementation(URCBehaviour* InBehaviour) const
{
	return false;
}

void URCBehaviourBlueprintNode::PreExecute_Implementation(URCBehaviour* InBehaviour) const
{
}

bool URCBehaviourBlueprintNode::IsSupported_Implementation(URCBehaviour* InBehaviour) const
{
	return false;
}

void URCBehaviourBlueprintNode::OnPassed_Implementation(URCBehaviour* InBehaviour) const
{
}
