// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesProcessor.h"

#include "Animation/AnimNodeBase.h"

#include "AnimNode_CorrectPose.generated.h"


USTRUCT(BlueprintInternalUseOnly)
struct POSECORRECTIVES_API FAnimNode_CorrectPose : public FAnimNode_Base
{
	GENERATED_BODY()

	FAnimNode_CorrectPose();
	
	/** Pose Correctives asset to use. */
	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPoseCorrectivesAsset> PoseCorrectivesAsset = nullptr;
	bool EditMode = false;

	/** Bones to use for driving parameters based on their transform */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return false; }
	// End of FAnimNode_Base interface

private:
	void UpdateRBFTargetsFromAsset();

	UPROPERTY(Transient)
	TObjectPtr<UPoseCorrectivesProcessor> PoseCorrectivesProcessor = nullptr;
	
	TArray<FCompactPoseBoneIndex> BoneCompactIndices;
	TArray<SmartName::UID_Type> CurveUIDs;

	TArray<FCorrectivesRBFTarget> RBFTargets;
	FCorrectivesRBFEntry RBFInput;
};
