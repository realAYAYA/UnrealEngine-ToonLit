// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Retargeter/IKRetargetOps.h"
#include "CurveRemapOp.generated.h"

#define LOCTEXT_NAMESPACE "CurveRemapOp"

USTRUCT(BlueprintType)
struct FCurveRemapPair
{
	GENERATED_BODY()
	
	// The curve name on the SOURCE skeletal mesh to copy animation data from.
	UPROPERTY(EditAnywhere, Category=Settings)
	FName SourceCurve;

	// The curve name on the TARGET skeletal mesh to receive animation data.
	UPROPERTY(EditAnywhere, Category=Settings)
	FName TargetCurve;
};

UCLASS(BlueprintType, EditInlineNew)
class IKRIG_API UCurveRemapOp : public URetargetOpBase
{
	GENERATED_BODY()

public:

	// this op does not do anything in Initialize() or Run(), but is used by the Retarget Pose from Mesh node and the Batch Exporter
	virtual bool Initialize(
	const UIKRetargetProcessor* Processor,
		const FRetargetSkeleton& SourceSkeleton,
		const FTargetSkeleton& TargetSkeleton,
		FIKRigLogger& Log) override { return true; };
	virtual void Run(
		const UIKRetargetProcessor* Processor,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) override {};

	// Add pairs of Source/Target curve names to remap. While retargeting, the animation from the source curves
	// will be redirected to the curves on the target skeletal meshes. Can be used to drive, blendshapes or other downstream systems.
	// NOTE: By default the IK Retargeter will automatically copy all equivalently named curves from the source to the
	// target. Remapping with this op is only necessary when the target curve name(s) are different.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	TArray<FCurveRemapPair> CurvesToRemap;

	// This setting only applies to all curves when exporting retargeted animations.
	// True: all source curves are copied to the target animation sequence asset.
	// False: only remapped curves are left on the target animation sequence asset.
	UPROPERTY(EditAnywhere, Category="OpSettings")
	bool bCopyAllSourceCurves = true;
	
#if WITH_EDITOR
	virtual FText GetNiceName() const override { return FText(LOCTEXT("OpName", "Remap Curves")); };
	virtual FText WarningMessage() const override { return Message; };
	FText Message;
#endif
};


#undef LOCTEXT_NAMESPACE
