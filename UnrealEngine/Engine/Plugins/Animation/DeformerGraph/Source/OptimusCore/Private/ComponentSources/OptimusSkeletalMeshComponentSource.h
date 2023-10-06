// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusSkinnedMeshComponentSource.h"

#include "OptimusSkeletalMeshComponentSource.generated.h"


UCLASS()
class UOptimusSkeletalMeshComponentSource :
	public UOptimusSkinnedMeshComponentSource
{
	GENERATED_BODY()
public:
	// UOptimusComponentSource implementations
	FText GetDisplayName() const override;
	FName GetBindingName() const override { return FName("SkeletalMesh"); }
	TSubclassOf<UActorComponent> GetComponentClass() const override;
};
