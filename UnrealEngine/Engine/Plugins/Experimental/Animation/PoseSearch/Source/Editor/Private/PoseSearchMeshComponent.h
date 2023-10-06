// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BonePose.h"
#include "Components/PoseableMeshComponent.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearchMeshComponent.generated.h"

class UBlendSpace;

UCLASS()
class UPoseSearchMeshComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:

	struct FUpdateContext
	{
		const UAnimSequenceBase* SequenceBase = nullptr;
		const UBlendSpace* BlendSpace = nullptr;
		float StartTime = 0.0f;
		float Time = 0.0f;
		bool bLoop = false;
		bool bMirrored = false;
		FVector BlendParameters = FVector::Zero();
		const UMirrorDataTable* MirrorDataTable = nullptr;
		TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>* CompactPoseMirrorBones = nullptr;
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex>* ComponentSpaceRefRotations = nullptr;
	};

	void Refresh();
	void ResetToStart();
	void UpdatePose(const FUpdateContext& UpdateContext);
	void Initialize(const FTransform& InComponentToWorld);
	FTransform StartingTransform = FTransform::Identity;
	FTransform LastRootMotionDelta = FTransform::Identity;
};
