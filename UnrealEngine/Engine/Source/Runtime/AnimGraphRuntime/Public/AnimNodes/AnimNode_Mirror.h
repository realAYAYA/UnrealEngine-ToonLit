// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/MirrorDataTable.h"
#include "AnimNode_Mirror.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_MirrorBase : public FAnimNode_Base
{
	GENERATED_BODY()
public:
	FAnimNode_MirrorBase(); 

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	// Get the MirrorDataTable
	virtual UMirrorDataTable* GetMirrorDataTable() const;

	// Set the MirrorDataTable
	virtual bool SetMirrorDataTable(UMirrorDataTable* MirrorTable);

	// Get Mirror State
	virtual bool GetMirror() const;
	// How long to blend using inertialization when switching  mirrored state
	virtual float GetBlendTimeOnMirrorStateChange() const;

	// Should bones mirror
	virtual bool GetBoneMirroring() const;

	// Should the curves mirror
	virtual bool GetCurveMirroring() const;

	// Should attributes mirror (based on the bone mirroring data in the mirror data table) 
	virtual bool GetAttributeMirroring() const;

	// Whether to reset (reinitialize) the child (source) pose when the mirror state changes
	virtual bool GetResetChildOnMirrorStateChange() const;

	// Set Mirror State
	virtual bool SetMirror(bool bInMirror);

	// Set how long to blend using inertialization when switching  mirrored state
	// @return true if the value was set (it is dynamic), or false if it could not (it is not dynamic or pin exposed)
	virtual bool SetBlendTimeOnMirrorStateChange(float InBlendTime);

	// Set if bones mirror
	// @return true if the value was set (it is dynamic), or false if it could not (it is not dynamic or pin exposed)
	virtual bool SetBoneMirroring(bool bInBoneMirroring);

	// Set if curves mirror
	// @return true if the value was set (it is dynamic), or false if it could not (it is not dynamic or pin exposed)
	virtual bool SetCurveMirroring(bool bInCurveMirroring);

	// Set if attributes mirror
	// @return true if the value was set (it is dynamic), or false if it could not (it is not dynamic or pin exposed)
	virtual bool SetAttributeMirroring(bool bInAttributeMirroring);

	// Set whether to reset (reinitialize) the child (source) pose when the mirror state changes
	// @return true if the value was set (it is dynamic), or false if it could not (it is not dynamic or pin exposed)
	virtual bool SetResetChildOnMirrorStateChange(bool bInResetChildOnMirrorStateChange);

	/** This only used by custom handlers, and it is advanced feature. */
	virtual void SetSourceLinkNode(FAnimNode_Base* NewLinkNode);

	/** This only used by custom handlers, and it is advanced feature. */
	virtual FAnimNode_Base* GetSourceLinkNode();
protected:
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

private:
	bool bMirrorState;
	bool bMirrorStateIsValid;

	void FillCompactPoseAndComponentRefRotations(const FBoneContainer& BoneContainer);
	// Compact pose format of Mirror Bone Map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	// Pre-calculated component space of reference pose, which allows mirror to work with any joint orient 
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;
};


USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_Mirror : public FAnimNode_MirrorBase
{
	GENERATED_BODY()

	friend class UAnimGraphNode_Mirror;

public:
	FAnimNode_Mirror();

	virtual UMirrorDataTable* GetMirrorDataTable() const override;
	virtual bool SetMirrorDataTable(UMirrorDataTable* MirrorTable) override;

	virtual bool GetMirror() const override;
	virtual float GetBlendTimeOnMirrorStateChange() const override;
	virtual bool GetBoneMirroring() const override;
	virtual bool GetCurveMirroring() const override;
	virtual bool GetAttributeMirroring() const override;
	virtual bool GetResetChildOnMirrorStateChange() const override;

	virtual bool SetMirror(bool bInMirror) override;
	virtual bool SetBlendTimeOnMirrorStateChange(float InBlendTime) override;
	virtual bool SetBoneMirroring(bool bInBoneMirroring) override;
	virtual bool SetCurveMirroring(bool bInCurveMirroring) override;
	virtual bool SetAttributeMirroring(bool bInAttributeMirroring) override;
	virtual bool SetResetChildOnMirrorStateChange(bool bInResetChildOnMirrorStateChange) override;

protected:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault, FoldProperty))
	bool bMirror = true; 

	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty))
	TObjectPtr<UMirrorDataTable> MirrorDataTable = nullptr;

	// Inertialization blend time to use when transitioning between mirrored and unmirrored states
	UPROPERTY(EditAnywhere, Category = MirrorTransition, meta = (PinHiddenByDefault, FoldProperty))
	float BlendTime = 0.0f;
	// Whether to reset (reinitialize) the child (source) pose when the mirror state changes
	UPROPERTY(EditAnywhere, Category = MirrorTransition, meta = (FoldProperty))
	bool bResetChild = false;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta=(DisplayName="Bone", FoldProperty))
	bool bBoneMirroring = true;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta=(DisplayName = "Curve", FoldProperty))
	bool bCurveMirroring = true;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta=(DisplayName = "Attributes", FoldProperty))
	bool bAttributeMirroring = true;
#endif
};


USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_Mirror_Standalone : public FAnimNode_MirrorBase
{
	GENERATED_BODY()

	friend class UAnimGraphNode_Mirror;

public:
	FAnimNode_Mirror_Standalone();

	virtual UMirrorDataTable* GetMirrorDataTable() const override;
	virtual bool SetMirrorDataTable(UMirrorDataTable* MirrorTable) override;

	virtual bool GetMirror() const override;
	virtual float GetBlendTimeOnMirrorStateChange() const override;
	virtual bool GetBoneMirroring() const override;
	virtual bool GetCurveMirroring() const override;
	virtual bool GetAttributeMirroring() const override;
	virtual bool GetResetChildOnMirrorStateChange() const override;

	virtual bool SetMirror(bool bInMirror) override;
	virtual bool SetBlendTimeOnMirrorStateChange(float InBlendTime) override;
	virtual bool SetBoneMirroring(bool bInBoneMirroring) override;
	virtual bool SetCurveMirroring(bool bInCurveMirroring) override;
	virtual bool SetAttributeMirroring(bool bInAttributeMirroring) override;
	virtual bool SetResetChildOnMirrorStateChange(bool bInResetChildOnMirrorStateChange) override;

protected:

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	bool bMirror = true;

	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UMirrorDataTable> MirrorDataTable = nullptr;

	// Inertialization blend time to use when transitioning between mirrored and unmirrored states
	UPROPERTY(EditAnywhere, Category = MirrorTransition, meta = (PinHiddenByDefault, FoldProperty))
	float BlendTime = 0.0f;

	// Whether to reset (reinitialize) the child (source) pose when the mirror state changes
	UPROPERTY(EditAnywhere, Category = MirrorTransition, meta = (FoldProperty))
	bool bResetChild = false;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta = (DisplayName = "Bone", FoldProperty))
	bool bBoneMirroring = true;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta = (DisplayName = "Curve", FoldProperty))
	bool bCurveMirroring = true;

	UPROPERTY(EditAnywhere, Category = MirroredChannels, meta = (DisplayName = "Attributes", FoldProperty))
	bool bAttributeMirroring = true;
};