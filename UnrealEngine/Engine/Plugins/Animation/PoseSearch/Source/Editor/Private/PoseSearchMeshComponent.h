// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PoseableMeshComponent.h"
#include "PoseSearchMeshComponent.generated.h"

UCLASS()
class UPoseSearchMeshComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:

	void Refresh();
	void Initialize(const FTransform& InComponentToWorld);
};
