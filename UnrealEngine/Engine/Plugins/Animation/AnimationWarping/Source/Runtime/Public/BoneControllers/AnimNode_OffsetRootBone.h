// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneControllers/BoneControllerTypes.h"
#include "BoneControllers/BoneControllerSolvers.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_OffsetRootBone.generated.h"

UENUM(BlueprintType)
enum class EOffsetRootBoneMode : uint8
{
	// Accumulate the mesh component's movement into the offset.
	// In this mode, if the mesh component moves 
	// the offset will counter the motion, and the root will stay in place
	Accumulate,
	// Continuously interpolate the offset out
	// In this mode, if the mesh component moves
	// The root will stay behind, but will attempt to catch up
	Interpolate,
	// Stops accumulating the mesh component's movement delta into the root offset
	// In this mode, whatever offset we have will be conserved, 
	// but we won't accumulate any more
	Hold,
	// Release the offset and stop accumulating the mesh component's movement delta.
	// In this mode we will "blend out" the offset
	Release,
};

USTRUCT(BlueprintInternalUseOnly, Experimental)
struct ANIMATIONWARPINGRUNTIME_API FAnimNode_OffsetRootBone : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY();

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Evaluation, meta=(FoldProperty))
	EWarpingEvaluationMode EvaluationMode = EWarpingEvaluationMode::Graph;

	// The translation offset behavior mode
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	EOffsetRootBoneMode TranslationMode = EOffsetRootBoneMode::Interpolate;

	// The rotation offset behavior mode
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	EOffsetRootBoneMode RotationMode = EOffsetRootBoneMode::Interpolate;

	// Controls how fast the translation offset is blended out
	// Values closer to 0 make it faster
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	float TranslationHalflife = 0.1f;

	// Controls how fast the rotation offset is blended out
	// Values closer to 0 make it faster
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	float RotationHalfLife = 0.2f;

	// How much the offset can deviate from the mesh component's translation in units
	// Values lower than 0 disable this limit
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	float MaxTranslationError = -1.0f;

	// How much the offset can deviate from the mesh component's rotation in degrees
	// Values lower than 0 disable this limit
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	float MaxRotationError = -1.0f;

	// Whether to limit the offset's translation interpolation speed to the velocity on the incoming motion
	// Enabling this prevents the offset sliding when there's little to no translation speed
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	bool bClampToTranslationVelocity = false;

	// Whether to limit the offset's rotation interpolation speed to the velocity on the incoming motion
	// Enabling this prevents the offset sliding when there's little to no rotation speed
	UPROPERTY(EditAnywhere, Category = Settings, meta = (FoldProperty, PinHiddenByDefault))
	bool bClampToRotationVelocity = false;

	// How much the offset can blend out, relative to the incoming translation speed
	// i.e. If root motion is moving at 400cm/s, at 0.5, the offset can blend out at 200cm/s
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = bClampToTranslationVelocity, FoldProperty, PinHiddenByDefault))
	float TranslationSpeedRatio = 0.5f;

	// How much the offset can blend out, relative to the incoming rotation speed
	// i.e. If root motion is rotating at 90 degrees/s, at 0.5, the offset can blend out at 45 degree/s
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = bClampToRotationVelocity, FoldProperty, PinHiddenByDefault))
	float RotationSpeedRatio = 0.5f;

	// Delta applied to the translation offset this frame. 
	// For procedural values, consider adjusting the input by delta time.
	UPROPERTY(EditAnywhere, Category = Evaluation, meta = (FoldProperty, PinHiddenByDefault))
	FVector TranslationDelta = FVector::ZeroVector;

	// Delta applied to the rotation offset this frame. 
	// For procedural values, consider adjusting the input by delta time.
	UPROPERTY(EditAnywhere, Category = Evaluation, meta = (FoldProperty, PinHiddenByDefault))
	FRotator RotationDelta = FRotator::ZeroRotator;
#endif

public:
	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	// Folded property accesors
	EWarpingEvaluationMode GetEvaluationMode() const;
	const FVector& GetTranslationDelta() const;
	const FRotator& GetRotationDelta() const;
	EOffsetRootBoneMode GetTranslationMode() const;
	EOffsetRootBoneMode GetRotationMode() const;
	float GetTranslationHalflife() const;
	float GetRotationHalfLife() const;
	float GetMaxTranslationError() const;
	float GetMaxRotationError() const;
	bool GetClampToTranslationVelocity() const;
	bool GetClampToRotationVelocity() const;
	float GetTranslationSpeedRatio() const;
	float GetRotationSpeedRatio() const;

private:

	void Reset(const FAnimationBaseContext& Context);

	// Internal cached anim instance proxy
	FAnimInstanceProxy* AnimInstanceProxy = nullptr;

	// Internal cached delta time used for interpolators
	float CachedDeltaTime = 0.f;

	bool bIsFirstUpdate = true;

	FTransform ComponentTransform = FTransform::Identity;

	// The simulated world-space transforms for the root bone with offset
	// Offset = ComponentTransform - SimulatedTransform
	FVector SimulatedTranslation = FVector::ZeroVector;
	FQuat SimulatedRotation = FQuat::Identity;

	FGraphTraversalCounter UpdateCounter;
};
