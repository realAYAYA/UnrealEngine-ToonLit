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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, transient, Category=Settings, meta=(PinHiddenByDefault))
	FRetargetProfile CustomRetargetProfile;

	/* Toggle whether to print warnings about missing or incorrectly configured retarget configurations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Debug, meta = (NeverAsPin))
	bool bSuppressWarnings = false;
	
	/* Copy curves from SouceMeshComponent. This will copy any curves the source/target Skeleton have in common. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (NeverAsPin))
	bool bCopyCurves = true;

#if WITH_EDITOR
	/** when true, will copy all setting from target IK Rig asset each tick (for live preview) */
	bool bDriveWithAsset = false;
#endif
	
	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

	/** Access to the runtime processor */
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
	// mapping of Skeleton curve names to the UID (for copying curves from the source mesh component)
	TMap<FName, SmartName::UID_Type> CurveNameToUIDMap;
	// cached curves, copied on the game thread
	TMap<FName, float> SourceCurveValues;

	/** update map of curve values containing speeds used for IK planting */
	void UpdateSpeedValuesFromCurves();
	// map of curve names to values, passed to retargeter for IK planting (copied from source mesh)
	TMap<FName, float> SpeedValuesFromCurves;
	// the accumulated delta time this tick
	float DeltaTime;
};
