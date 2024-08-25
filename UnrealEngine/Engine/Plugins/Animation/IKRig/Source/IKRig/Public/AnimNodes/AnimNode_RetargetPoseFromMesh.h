// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimNodeBase.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargetProfile.h"

#include "AnimNode_RetargetPoseFromMesh.generated.h"

DECLARE_CYCLE_STAT(TEXT("IK Retarget"), STAT_IKRetarget, STATGROUP_Anim);

USTRUCT(BlueprintInternalUseOnly)
struct IKRIG_API FAnimNode_RetargetPoseFromMesh : public FAnimNode_Base
{
	GENERATED_BODY()

	/** The Skeletal Mesh Component to retarget animation from. Assumed to be animated and tick BEFORE this anim instance.*/
	UPROPERTY(BlueprintReadWrite, transient, Category=Settings, meta=(PinShownByDefault))
	TWeakObjectPtr<USkeletalMeshComponent> SourceMeshComponent = nullptr;

	/* If SourceMeshComponent is not valid, and if this is true, it will look for attached parent as a source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(NeverAsPin))
	bool bUseAttachedParent = true;
	
	/** Retarget asset to use. Must define a Source and Target IK Rig compatible with the SourceMeshComponent and current anim instance.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(PinHiddenByDefault))
	TObjectPtr<UIKRetargeter> IKRetargeterAsset = nullptr;

	/** connect a custom retarget profile to modify the retargeter's settings at runtime.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	FRetargetProfile CustomRetargetProfile;

	/* Toggle whether to print warnings about missing or incorrectly configured retarget configurations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Debug, meta = (NeverAsPin))
	bool bSuppressWarnings = false;
	
	/* Copy curves from SouceMeshComponent. This will copy any curves the source/target Skeleton have in common. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (NeverAsPin))
	bool bCopyCurves = true;

	/*
	* Max LOD that this node is allowed to run.
	* For example if you have LODThreshold at 2, it will run until LOD 2 (based on 0 index) when the component LOD becomes 3, it will stop update/evaluate
	* A value of -1 forces the node to execute at all LOD levels.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold = -1;

	/*
	* Max LOD that IK is allowed to run.
	* For example if you have LODThresholdForIK at 2, it will skip the IK pass on LODs 3 and greater.
	* This only disables IK and does not affect the Root or FK passes.
	* A value of -1 forces the node to execute at all LOD levels.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Performance, meta = (DisplayName = "IK LOD Threshold"))
	int32 LODThresholdForIK = -1;
	
	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	// End of FAnimNode_Base interface

	/** Access to the runtime processor */
	void CreateRetargetProcessorIfNeeded(UObject* Outer);
	UIKRetargetProcessor* GetRetargetProcessor() const;

private:

	/** returns true if processor is setup and ready to go, false otherwise */
	bool EnsureProcessorIsInitialized(const TObjectPtr<USkeletalMeshComponent> TargetMeshComponent);
	/** copies the source mesh pose (on main thread) */
	void CopyBoneTransformsFromSource(USkeletalMeshComponent* TargetMeshComponent);
	/** indirection to account for Leader Pose Component setup */
	TObjectPtr<USkeletalMeshComponent> GetComponentToCopyPoseFrom() const;

	/** the runtime processor used to run the retarget and generate new poses */
	UPROPERTY(Transient)
	TObjectPtr<UIKRetargetProcessor> Processor = nullptr;

	// cached transforms, copied on the game thread
	TArray<FTransform> SourceMeshComponentSpaceBoneTransforms;

	// mapping from required bones to actual bones within the target skeleton
	TArray< TPair<int32, int32> > RequiredToTargetBoneMapping;

	// cached curves, copied on the game thread in PreUpdate()
	FBlendedHeapCurve SourceCurves;

	// remap curves for CurveRemapOp if one is present in the RetargetOp stack
	void CopyAndRemapCurvesFromSourceToTarget(FBlendedCurve& OutputCurves) const;
	
	// update map of curve values containing speeds used for IK planting
	void UpdateSpeedValuesFromCurves();
	// map of curve names to values, passed to retargeter for IK planting (copied from source mesh)
	TMap<FName, float> SpeedValuesFromCurves;
	// the accumulated delta time this tick
	float DeltaTime;
};
