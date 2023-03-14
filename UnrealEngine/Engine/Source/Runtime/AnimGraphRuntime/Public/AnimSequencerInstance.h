// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

#pragma once
#include "Animation/AnimInstance.h"
#include "SequencerAnimationSupport.h"
#include "AnimSequencerInstance.generated.h"

struct FRootMotionOverride;
struct FAnimSequencerData;

UCLASS(transient, NotBlueprintable)
class ANIMGRAPHRUNTIME_API UAnimSequencerInstance : public UAnimInstance, public ISequencerAnimationSupport
{
	GENERATED_UCLASS_BODY()

public:

	/** Update an animation sequence player in this instance */
	virtual void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InPosition, float Weight, bool bFireNotifies);
	virtual void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies);

	/** Update with Root Motion*/
	UE_DEPRECATED(5.0, "Please use UpdateAnimTrackWithRootMotion that takes a MirrorDataTable")
	void UpdateAnimTrackWithRootMotion(UAnimSequenceBase* InAnimSequence, int32 SequenceId, const TOptional<FRootMotionOverride>& RootMotion, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies);

	UE_DEPRECATED(5.1, "Please use the UpdateAnimTrackWithRootMotion that takes FAnimSequencerData")
	void UpdateAnimTrackWithRootMotion(UAnimSequenceBase* InAnimSequence, int32 SequenceId,const TOptional<FRootMotionOverride>& RootMotion, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies, UMirrorDataTable* InMirrorDataTable);
	
	void UpdateAnimTrackWithRootMotion(const FAnimSequencerData& InAnimSequencerData);

	/** Construct all nodes in this instance */
	virtual void ConstructNodes() override;

	/** Reset all nodes in this instance */
	virtual void ResetNodes() override;

	/** Reset the pose for this instance*/
	virtual void ResetPose() override;

	/** Saved the named pose to restore after */
	virtual void SavePose() override;

	virtual UAnimInstance* GetSourceAnimInstance() override { return this; }
	virtual void SetSourceAnimInstance(UAnimInstance* SourceAnimInstance) {  /* nothing to do */ ensure(false); }
	virtual bool DoesSupportDifferentSourceAnimInstance() const override { return false; }

protected:
	// UAnimInstance interface
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;



public:
	static const FName SequencerPoseName;

};

