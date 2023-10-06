// Copyright Epic Games, Inc. All Rights Reserved.

#include "Behaviour/Builtin/Bind/RCBehaviourBindNode.h"

#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"

#define LOCTEXT_NAMESPACE "RemoteControlBehaviours"

URCBehaviourBindNode::URCBehaviourBindNode()
{
	DisplayName = LOCTEXT("BehaviorNameBind", "Bind");
	BehaviorDescription = LOCTEXT("BehaviorDescBind", "Bind exposed properties to match the value of the controller. Properties added to the action list will not receive input values");
}

bool URCBehaviourBindNode::Execute_Implementation(URCBehaviour* InBehaviour) const
{
	return true;
}

bool URCBehaviourBindNode::IsSupported_Implementation(URCBehaviour* InBehaviour) const
{
	return true; 
}

UClass* URCBehaviourBindNode::GetBehaviourClass() const
{
	return URCBehaviourBind::StaticClass();
}

void URCBehaviourBindNode::OnPassed_Implementation(URCBehaviour* InBehaviour) const
{
}

#undef LOCTEXT_NAMESPACE