// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolTargets/ToolTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ToolTarget)

bool FToolTargetTypeRequirements::AreSatisfiedBy(UClass* Class) const
{
	// we have to support all the required interfaces
	for (const UClass* Interface : Interfaces)
	{
		if (!Class->ImplementsInterface(Interface))
		{
			return false;
		}
	}
	return true;
}

bool FToolTargetTypeRequirements::AreSatisfiedBy(UToolTarget* ToolTarget) const
{
	return !ToolTarget ? false : AreSatisfiedBy(ToolTarget->GetClass());
}
