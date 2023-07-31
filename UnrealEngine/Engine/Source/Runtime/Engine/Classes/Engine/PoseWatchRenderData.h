// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BoneIndices.h"

struct FReferenceSkeleton;

struct ENGINE_API FAnimNodePoseWatch
{
public:
	FAnimNodePoseWatch();

	// Object (anim instance) that this pose came from
	const UObject* Object;
	class UPoseWatch* PoseWatch;
	class UPoseWatchPoseElement* PoseWatchPoseElement;
	int32 NodeID;

	bool IsValid() const;

	template<class TAllocator>
	bool SetPose(const TArray<FBoneIndexType>& InRequiredBones, const TArray<FTransform, TAllocator>& InBoneTransforms)
	{
		RequiredBones = InRequiredBones;
		BoneTransforms = InBoneTransforms;
		return true;
	}

	void SetWorldTransform(const FTransform& InWorldTransform);

	/**
	 * Take a snapshot of the pose watch properties that this struct represents
	 * to be used when drawing the debug skeleton
	 */
	void CopyPoseWatchData(const FReferenceSkeleton& RefSkeleton);

	const TArray<FBoneIndexType>& GetRequiredBones() const;

	const TArray<FTransform>& GetBoneTransforms() const;

	const FTransform& GetWorldTransform() const;

	FLinearColor GetBoneColor() const;

	FVector GetViewportOffset() const;

	TArray<int32> GetViewportAllowList() const;

	TArray<int32> GetParentIndices() const;

private:
	FTransform WorldTransform;
	TArray<FBoneIndexType> RequiredBones;
	TArray<FTransform> BoneTransforms;

	// Mirrored properties updated on CopyPoseWatchData
	FLinearColor BoneColor;
	FVector ViewportOffset;
	TArray<int32> ViewportMaskAllowedList;
	TArray<int32> ParentIndices;
};

#endif