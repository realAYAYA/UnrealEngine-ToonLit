// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/AimAssistTargetComponent.h"

#include "Input/IAimAssistTargetInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimAssistTargetComponent)

void UAimAssistTargetComponent::GatherTargetOptions(FAimAssistTargetOptions& OutTargetData)
{
	if (!TargetData.TargetShapeComponent.IsValid())
	{
		if (AActor* Owner = GetOwner())
		{
			TargetData.TargetShapeComponent = Owner->FindComponentByClass<UShapeComponent>();	
		}
	}
	OutTargetData = TargetData;
}

