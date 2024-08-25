// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "AnimNextMeshComponent.generated.h"

struct FAnimNextGraphReferencePose;
class UAnimNextSchedulePort_AnimNextMeshComponentPose;

// Mesh component for use with AnimNext
UCLASS(MinimalAPI, Blueprintable, meta = (BlueprintSpawnableComponent))
class UAnimNextMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

	friend class UAnimNextSchedulePort_AnimNextMeshComponentPose;

	UAnimNextMeshComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	// Converts local to component space, flips buffers, updates bounds and dispatches to renderer
	void CompleteAndDispatch(TConstArrayView<FBoneIndexType> InParentIndices, TConstArrayView<FBoneIndexType> InRequiredBoneIndices, TConstArrayView<FTransform> InLocalSpaceTransforms);

	// Access the ref pose of the component
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, meta=(BlueprintThreadSafe))
	FAnimNextGraphReferencePose GetReferencePose();
};