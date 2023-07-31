// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstanceProxy.h"
#include "AnimNode_ControlRig_ExternalSource.h"
#include "ControlRigLayerInstanceProxy.generated.h"

class UControlRig;
class UAnimSequencerInstance;

/** Custom internal Input Pose node that handles any AnimInstance */
USTRUCT()
struct FAnimNode_ControlRigInputPose : public FAnimNode_Base
{
	GENERATED_BODY()

	FAnimNode_ControlRigInputPose()
		: InputProxy(nullptr)
	{
	}

	/** Input pose, optionally linked dynamically to another graph */
	UPROPERTY()
	FPoseLink InputPose;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	/** Called by linked instance nodes to dynamically link this to an outer graph */
	void Link(UAnimInstance* InInputInstance, FAnimInstanceProxy* InInputProxy);

	/** Called by linked instance nodes to dynamically unlink this to an outer graph */
	void Unlink();

private:
	/** The proxy to use when getting inputs, set when dynamically linked */
	FAnimInstanceProxy* InputProxy;
	UAnimInstance*		InputAnimInstance;
};

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct CONTROLRIG_API FControlRigLayerInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FControlRigLayerInstanceProxy()
		: CurrentRoot(nullptr)
		, CurrentSourceAnimInstance(nullptr)
	{
	}

	FControlRigLayerInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
		, CurrentRoot(nullptr)
		, CurrentSourceAnimInstance(nullptr)
	{
	}

	virtual ~FControlRigLayerInstanceProxy();

	// FAnimInstanceProxy interface
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void CacheBones() override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

	/** Anim Instance Source info - created externally and used here */
	void SetSourceAnimInstance(UAnimInstance* SourceAnimInstance, FAnimInstanceProxy* SourceAnimInputProxy);
	UAnimInstance* GetSourceAnimInstance() const { return CurrentSourceAnimInstance; }

	/** ControlRig related support */
	void AddControlRigTrack(int32 ControlRigID, UControlRig* InControlRig);
	void UpdateControlRigTrack(int32 ControlRigID, float Weight, const FControlRigIOSettings& InputSettings, bool bExecute);
	void RemoveControlRigTrack(int32 ControlRigID);
	bool HasControlRigTrack(int32 ControlRigID);
	void ResetControlRigTracks();

	/** Sequencer AnimInstance Interface */
	void AddAnimation(int32 SequenceId, UAnimSequenceBase* InAnimSequence);
	void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InPosition, float Weight, bool bFireNotifies);
	void UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies);
	void RemoveAnimation(int32 SequenceId);

	/** Reset all nodes in this instance */
	virtual void ResetNodes();
	/** Reset the pose in this instance*/
	virtual void ResetPose();
	/** Construct and link the base part of the blend tree */
	virtual void ConstructNodes();

	/** return first available control rig from the node it has */
	UControlRig* GetFirstAvailableControlRig() const;

	virtual void AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector) override;

	// this doesn't work because this instance continuously change root
	// this will invalidate the evaluation
// 	virtual FAnimNode_Base* GetCustomRootNode() 

	friend struct FAnimNode_ControlRigInputPose;
protected:
	/** Find ControlRig node of the */
	FAnimNode_ControlRig_ExternalSource* FindControlRigNode(int32 ControlRigID) const;

	/** Input pose anim node */
	FAnimNode_ControlRigInputPose InputPose;

	/** Cuyrrent Root node - this changes whenever track changes */
	FAnimNode_Base* CurrentRoot;

	/** ControlRig Nodes */
	TArray<TSharedPtr<FAnimNode_ControlRig_ExternalSource>> ControlRigNodes;

	/** mapping from sequencer index to internal player index */
	TMap<int32, FAnimNode_ControlRig_ExternalSource*> SequencerToControlRigNodeMap;

	/** Source Anim Instance */
	UAnimInstance* CurrentSourceAnimInstance;

	/** getter for Sequencer AnimInstance. It will return null if it's using AnimBP */
	UAnimSequencerInstance* GetSequencerAnimInstance();

	static void InitializeCustomProxy(FAnimInstanceProxy* InputProxy, UAnimInstance* InAnimInstance);
	static void GatherCustomProxyDebugData(FAnimInstanceProxy* InputProxy, FNodeDebugData& DebugData);
	static void CacheBonesCustomProxy(FAnimInstanceProxy* InputProxy);
	static void UpdateCustomProxy(FAnimInstanceProxy* InputProxy, const FAnimationUpdateContext& Context);
	static void EvaluateCustomProxy(FAnimInstanceProxy* InputProxy, FPoseContext& Output);
	/** reset internal Counters of given animinstance proxy */
	static void ResetCounter(FAnimInstanceProxy* InAnimInstanceProxy);
};
