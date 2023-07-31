// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusSkeletalMeshComponentSource.h"

#include "Components/SkeletalMeshComponent.h"


#define LOCTEXT_NAMESPACE "OptimusSkeletalMeshComponentSource"


TSubclassOf<UActorComponent> UOptimusSkeletalMeshComponentSource::GetComponentClass() const
{
	return USkeletalMeshComponent::StaticClass();
}


FText UOptimusSkeletalMeshComponentSource::GetDisplayName() const
{
	return LOCTEXT("SkeletalMeshComponent", "Skeletal Mesh Component");
}


#undef LOCTEXT_NAMESPACE
