// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Retargeter/IKRetargetOps.h"

#include "PinBoneOp.generated.h"

#define LOCTEXT_NAMESPACE "RetargetOps"

// which skeleton are we referring to?
UENUM()
enum class EPinBoneType : uint8
{
	FullTransform,
	TranslateOnly,
	RotateOnly,
	ScaleOnly
};

USTRUCT(BlueprintType)
struct FPinBoneData
{
	GENERATED_BODY()

	FPinBoneData() = default;
	
	FPinBoneData(FName InBoneToPin, FName InBoneToPinTo)
	: BoneToPin(InBoneToPin)
	, BoneToPinTo(InBoneToPinTo)
	, BoneToPinIndex(INDEX_NONE)
	, BoneToPinToIndex(INDEX_NONE)
	, OffsetInRefPose(FTransform::Identity){}

	// The bone to be affected by this op. Will have it's transform modified to match the BoneToPinTo
	UPROPERTY(EditAnywhere, Category=Settings)
	FName BoneToPin;

	// The bone, on the target skeleton to pin to.
	UPROPERTY(EditAnywhere, Category=Settings)
	FName BoneToPinTo;
	
	int32 BoneToPinIndex;
	int32 BoneToPinToIndex;
	FTransform OffsetInRefPose;
};

UCLASS(BlueprintType, EditInlineNew)
class IKRIG_API UPinBoneOp : public URetargetOpBase
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

	UPROPERTY(EditAnywhere, Category="OpSettings")
	TArray<FPinBoneData> BonesToPin;

	// The bone, on the target skeleton to pin to.
	UPROPERTY(EditAnywhere, Category=Settings)
	ERetargetSourceOrTarget PinTo = ERetargetSourceOrTarget::Target;

	// Apply this pin to the full transform, or just translation or rotation only.
	UPROPERTY(EditAnywhere, Category=Settings)
	EPinBoneType PinType = EPinBoneType::FullTransform;

	// Maintain the original offset between the BoneToPin and BoneToPinTo
	UPROPERTY(EditAnywhere, Category=Settings)
	bool bMaintainOffset = true;

	// A manual offset to apply in global space
	UPROPERTY(EditAnywhere, Category=Settings)
	FTransform GlobalOffset;

	// A manual offset to apply in local space
	UPROPERTY(EditAnywhere, Category=Settings)
	FTransform LocalOffset;
	
#if WITH_EDITOR
	virtual FText GetNiceName() const override { return FText(LOCTEXT("OpName", "Pin Bone")); };
	virtual FText WarningMessage() const override { return Message; };
	FText Message;
#endif
};

#undef LOCTEXT_NAMESPACE
