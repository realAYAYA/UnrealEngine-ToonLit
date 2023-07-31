// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_IsInteracting.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_IsInteracting)

FRigUnit_IsInteracting_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	bIsInteracting = Context.IsInteracting();
	bIsTranslating = ( Context.InteractionType & (uint8)EControlRigInteractionType::Translate) != 0;
	bIsRotating = ( Context.InteractionType & (uint8)EControlRigInteractionType::Rotate) != 0;
	bIsScaling = ( Context.InteractionType & (uint8)EControlRigInteractionType::Scale) != 0;
	Items = Context.ElementsBeingInteracted;
}

