// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

#pragma once
#include "Animation/AnimInstance.h"
#include "SequencerAnimationSupport.h"
#include "AnimNode_ControlRigBase.h"
#include "ControlRigLayerInstance.generated.h"

class UControlRig;

UCLASS(transient, NotBlueprintable)
class CONTROLRIG_API UControlRigLayerInstance : public UAnimInstance, public ISequencerAnimationSupport
{
	GENERATED_UCLASS_BODY()

public:
	/** ControlRig related support */
	void AddControlRigTrack(int32 ControlRigID, UControlRig* InControlRig);
	void UpdateControlRigTrack(int32 ControlRigID, float Weight, const FControlRigIOSettings& InputSettings, bool bExecute);
	void RemoveControlRigTrack(int32 ControlRigID);
	bool HasControlRigTrack(int32 ControlRigID);
	void ResetControlRigTracks();

	/** Sequencer AnimInstance Interface */
	void AddAnimation(int32 SequenceId, UAnimSequenceBase* InAnimSequence);
	virtual void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InPosition, float Weight, bool bFireNotifies) override;
	virtual void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies) override;
	void RemoveAnimation(int32 SequenceId);

	/** Construct all nodes in this instance */
	virtual void ConstructNodes() override;
	/** Reset all nodes in this instance */
	virtual void ResetNodes() override;
	/** Reset the pose in this instance*/
	virtual void ResetPose() override;
	/** Saved the named pose to restore after */
	virtual void SavePose() override;

	/** Return the first available control rig */
	UControlRig* GetFirstAvailableControlRig() const;

	virtual UAnimInstance* GetSourceAnimInstance() override;
	virtual void SetSourceAnimInstance(UAnimInstance* SourceAnimInstance) override;
	virtual bool DoesSupportDifferentSourceAnimInstance() const override { return true; }

protected:
	// UAnimInstance interface
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

public:
	static const FName SequencerPoseName;
};
