// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ControllerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerComponent)

UControllerComponent::UControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UControllerComponent::IsLocalController() const
{
	return GetControllerChecked<AController>()->IsLocalController();
}

void UControllerComponent::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
	GetControllerChecked<AController>()->GetPlayerViewPoint(Location, Rotation);
}
