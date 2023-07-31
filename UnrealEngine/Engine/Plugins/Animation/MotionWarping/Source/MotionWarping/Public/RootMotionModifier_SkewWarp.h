// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "RootMotionModifier_SkewWarp.generated.h"

UCLASS(meta = (DisplayName = "Skew Warp"))
class MOTIONWARPING_API URootMotionModifier_SkewWarp : public URootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	URootMotionModifier_SkewWarp(const FObjectInitializer& ObjectInitializer);

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;

	static FVector WarpTranslation(const FTransform& CurrentTransform, const FVector& DeltaTranslation, const FVector& TotalTranslation, const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static URootMotionModifier_SkewWarp* AddRootMotionModifierSkewWarp(
		UPARAM(DisplayName = "Motion Warping Comp") UMotionWarpingComponent* InMotionWarpingComp,
		UPARAM(DisplayName = "Animation") const UAnimSequenceBase* InAnimation,
		UPARAM(DisplayName = "Start Time") float InStartTime,
		UPARAM(DisplayName = "End Time") float InEndTime,
		UPARAM(DisplayName = "Warp Target Name") FName InWarpTargetName,
		UPARAM(DisplayName = "Warp Point Anim Provider") EWarpPointAnimProvider InWarpPointAnimProvider,
		UPARAM(DisplayName = "Warp Point Anim Transform") FTransform InWarpPointAnimTransform,
		UPARAM(DisplayName = "Warp Point Anim Bone Name") FName InWarpPointAnimBoneName,
		UPARAM(DisplayName = "Warp Translation") bool bInWarpTranslation = true,
		UPARAM(DisplayName = "Ignore Z Axis") bool bInIgnoreZAxis = true,
		UPARAM(DisplayName = "Warp Rotation") bool bInWarpRotation = true,
		UPARAM(DisplayName = "Rotation Type") EMotionWarpRotationType InRotationType = EMotionWarpRotationType::Default,
		UPARAM(DisplayName = "Warp Rotation Time Multiplier") float InWarpRotationTimeMultiplier = 1.f);
};