// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchMeshComponent.h"
#include "Engine/SkinnedAsset.h"

void UPoseSearchMeshComponent::Initialize(const FTransform& InComponentToWorld)
{
	SetComponentToWorld(InComponentToWorld);
	const FReferenceSkeleton& SkeletalMeshRefSkeleton = GetSkinnedAsset()->GetRefSkeleton();

	// set up bone visibility states as this gets skipped since we allocate the component array before registration
	for (int32 BaseIndex = 0; BaseIndex < 2; ++BaseIndex)
	{
		BoneVisibilityStates[BaseIndex].SetNum(SkeletalMeshRefSkeleton.GetNum());
		for (int32 BoneIndex = 0; BoneIndex < SkeletalMeshRefSkeleton.GetNum(); BoneIndex++)
		{
			BoneVisibilityStates[BaseIndex][BoneIndex] = BVS_ExplicitlyHidden;
		}
	}

	Refresh();
}

void UPoseSearchMeshComponent::Refresh()
{
	// Flip buffers once to copy the directly-written component space transforms
	bNeedToFlipSpaceBaseBuffers = true;
	bHasValidBoneTransform = false;
	FlipEditableSpaceBases();
	bHasValidBoneTransform = true;

	InvalidateCachedBounds();
	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();
	MarkRenderStateDirty();
}