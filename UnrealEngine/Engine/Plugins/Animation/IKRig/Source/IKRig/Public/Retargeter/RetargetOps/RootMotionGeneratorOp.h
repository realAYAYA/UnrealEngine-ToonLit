// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetOps.h"

#include "RootMotionGeneratorOp.generated.h"

#define LOCTEXT_NAMESPACE "URootMotionGeneratorOp"

// where to copy the motion of the root from
UENUM()
enum class ERootMotionSource : uint8
{
	CopyFromSourceRoot,
	GenerateFromTargetPelvis,
};

// where to copy the height of the root from
UENUM()
enum class ERootMotionHeightSource : uint8
{
	CopyHeightFromSource,
	SnapToGround,
};

UCLASS(BlueprintType, EditInlineNew)
class IKRIG_API URootMotionGeneratorOp : public URetargetOpBase
{
	GENERATED_BODY()

public:
	
	virtual bool Initialize(
		const UIKRetargetProcessor* Processor,
		const FRetargetSkeleton& SourceSkeleton,
		const FTargetSkeleton& TargetSkeleton,
		FIKRigLogger& Log) override;
	
	virtual void Run(
		const UIKRetargetProcessor* Processor,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) override;

	// The root of the source skeleton.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	FName SourceRootBone;

	// The root of the target skeleton.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	FName TargetRootBone;

	// The pelvis of the target skeleton.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	FName TargetPelvisBone;

	// Where to copy the root motion from.
	// Copy From Source Root: copies the motion from the source root bone, but scales it according to relative height difference.
	// Generate From Target Pelvis: uses the retargeted Pelvis motion to generate root motion.
	// NOTE: Generating root motion from the Pelvis 
	UPROPERTY(EditAnywhere, Category="OpSettings")
	ERootMotionSource RootMotionSource;
	
	// How to set the height of the root bone.
	// Copy Height From Source: copies the height of the root bone on the source skeleton's root bone.
	// Snap To Ground: sets the root bone height to the ground plane (Z=0).
	UPROPERTY(EditAnywhere, Category="OpSettings")
	ERootMotionHeightSource RootHeightSource;

	// Will transform all children of the target root that are not themselves part of a retarget chain.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	bool bPropagateToNonRetargetedChildren = true;

	// Applies only when generating root motion from the Pelvis.
	// Maintains the offset between the root and pelvis as recorded in the target retarget pose.
	// If false, the root bone is placed directly under the Pelvis bone.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	bool bMaintainOffsetFromPelvis = true;

	// Applies only when generating root motion from the Pelvis.
	// When true, the applied offset will be rotated by the Pelvis.
	// (NOTE: This may cause unwanted rotations, for example if Pelvis Yaw is animated.)
	UPROPERTY(EditAnywhere, Category="OpSettings")
	bool bRotateWithPelvis = false;

	// A manual offset to apply in global space to the root bone.
	UPROPERTY(EditAnywhere, Category=Settings)
	FTransform GlobalOffset;

#if WITH_EDITOR
	virtual void OnAddedToStack(const UIKRetargeter* Asset) override;
	virtual FText GetNiceName() const override { return FText(LOCTEXT("OpName", "Root Motion Generator")); };
	virtual FText WarningMessage() const override { return Message; };
	FText Message;
#endif

private:

	void GenerateRootMotionFromTargetPelvis(
		FTransform& OutRootTransform,
		const TArray<FTransform>& InSourceGlobalPose,
		const TArray<FTransform>& InTargetGlobalPose) const;
	
	void CopyRootMotionFromSourceRoot(
		FTransform& OutRootTransform,
		const UIKRetargetProcessor* Processor,
		const TArray<FTransform>& InSourceGlobalPose) const;
	
	int32 SourceRootIndex;
	int32 TargetRootIndex;
	int32 TargetPelvisIndex;

	FTransform TargetPelvisRelativeToTargetRootRefPose;
	FTransform TargetPelvisInRefPose;
	FTransform SourceRootInRefPose;
	FTransform TargetRootInRefPose;
	
	TArray<int32> NonRetargetedChildrenOfRoot;
};

#undef LOCTEXT_NAMESPACE
