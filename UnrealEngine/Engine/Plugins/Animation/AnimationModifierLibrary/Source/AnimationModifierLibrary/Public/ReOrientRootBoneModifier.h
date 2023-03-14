// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "ReOrientRootBoneModifier.generated.h"

/** Reorient root bone in the animation while maintaining mesh position and rotation */
UCLASS()
class UReOrientRootBoneModifier : public UAnimationModifier
{
	GENERATED_BODY()

public:

	/** Rotation to apply to the root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FRotator Rotator;

	UReOrientRootBoneModifier();

	virtual void OnApply_Implementation(UAnimSequence* Animation) override;
	virtual void OnRevert_Implementation(UAnimSequence* Animation) override;

private:

	void ReOrientRootBone_Internal(UAnimSequence* Animation, const FQuat& Quat);

};
