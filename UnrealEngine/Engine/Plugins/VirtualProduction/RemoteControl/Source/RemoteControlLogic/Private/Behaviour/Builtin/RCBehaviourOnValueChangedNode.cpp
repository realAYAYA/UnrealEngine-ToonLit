// Copyright Epic Games, Inc. All Rights Reserved.

#include "Behaviour/Builtin/RCBehaviourOnValueChangedNode.h"

#include "PropertyBag.h"

URCBehaviourOnValueChangedNode::URCBehaviourOnValueChangedNode()
{
	DisplayName = NSLOCTEXT("Remote Control Behaviour", "Behavior Name - On Value Changed", "On Modify");
	BehaviorDescription = NSLOCTEXT("Remote Control Behaviour", "Behavior Desc - On Value Changed", "Triggers an event when the associated property is modified");
}


bool URCBehaviourOnValueChangedNode::Execute_Implementation(URCBehaviour* InBehaviour) const
{
	return true;
}

bool URCBehaviourOnValueChangedNode::IsSupported_Implementation(URCBehaviour* InBehaviour) const
{
	static TArray<TPair<EPropertyBagPropertyType, UObject*>> SupportedPropertyBagTypes =
	{
		{ EPropertyBagPropertyType::Bool, nullptr },
		{ EPropertyBagPropertyType::Byte, nullptr },
		{ EPropertyBagPropertyType::Int32, nullptr },
		{ EPropertyBagPropertyType::Float, nullptr },
		{ EPropertyBagPropertyType::Double, nullptr },
		{ EPropertyBagPropertyType::Name, nullptr },
		{ EPropertyBagPropertyType::String, nullptr },
		{ EPropertyBagPropertyType::Text, nullptr },
		{ EPropertyBagPropertyType::Struct, TBaseStructure<FVector>::Get() },
		{ EPropertyBagPropertyType::Struct, TBaseStructure<FVector2D>::Get() },
		{ EPropertyBagPropertyType::Struct, TBaseStructure<FColor>::Get() },
		{ EPropertyBagPropertyType::Struct, TBaseStructure<FRotator>::Get() }
	};
	
	return SupportedPropertyBagTypes.ContainsByPredicate(GetIsSupportedCallback(InBehaviour));
}
