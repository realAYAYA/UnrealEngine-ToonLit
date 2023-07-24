// Copyright Epic Games, Inc. All Rights Reserved.

#include "Behaviour/RCBehaviourNode.h"
#include "RCVirtualProperty.h"
#include "Behaviour/RCBehaviour.h"
#include "Controller/RCController.h"

void URCBehaviourNode::PreExecute(URCBehaviour* InBehaviour) const
{
}

bool URCBehaviourNode::Execute(URCBehaviour* InBehaviour) const
{
	return false;
}

bool URCBehaviourNode::IsSupported(URCBehaviour* InBehaviour) const
{
	return false;
}

void URCBehaviourNode::OnPassed(URCBehaviour* InBehaviour) const
{
}

UClass* URCBehaviourNode::GetBehaviourClass() const
{
	return URCBehaviour::StaticClass();
}

TFunction<bool(const TPair<EPropertyBagPropertyType, UObject*>)> URCBehaviourNode::GetIsSupportedCallback(URCBehaviour* InBehaviour)
{
	return[InBehaviour](const TPair<EPropertyBagPropertyType, UObject*> InPair)
	{
		if (const URCController* RCController = InBehaviour->ControllerWeakPtr.Get())
		{
			return RCController->GetValueType() == InPair.Key && RCController->GetValueTypeObjectWeakPtr() == InPair.Value;
		}
		
		return false;
	};
}
