// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNode_CorrectivesSource.generated.h"

class UPoseCorrectivesAsset;

USTRUCT()
struct FAnimNode_CorrectivesSource : public FAnimNode_Base
{
	GENERATED_BODY()

	FPoseLink SourcePose;

	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;

	UPoseCorrectivesAsset* PoseCorrectivesAsset;
	FName CurrentCorrrective;

	bool bUseCorrectiveSource = false;
	bool bUseSourcePose = true;

private:

	TArray<FCompactPoseBoneIndex> BoneCompactIndices;
	TArray<SmartName::UID_Type> CurveUIDs;
};