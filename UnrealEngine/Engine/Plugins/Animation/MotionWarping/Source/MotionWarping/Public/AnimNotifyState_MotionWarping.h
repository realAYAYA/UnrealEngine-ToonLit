// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MotionWarping.generated.h"

class UMotionWarpingComponent;
class UAnimSequenceBase;
class URootMotionModifier;

/** AnimNotifyState used to define a motion warping window in the animation */
UCLASS(meta = (DisplayName = "Motion Warping"))
class MOTIONWARPING_API UAnimNotifyState_MotionWarping : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	//@TODO: Prevent notify callbacks and add comments explaining why we don't use those here.

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Config")
	TObjectPtr<URootMotionModifier> RootMotionModifier;

	UAnimNotifyState_MotionWarping(const FObjectInitializer& ObjectInitializer);

	/** Called from the MotionWarpingComp when this notify becomes relevant. See: UMotionWarpingComponent::Update */
	void OnBecomeRelevant(UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	/** Creates a root motion modifier from the config class defined in the notify */
	UFUNCTION(BlueprintNativeEvent, Category = "Motion Warping")
	URootMotionModifier* AddRootMotionModifier(UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	UFUNCTION()
	void OnRootMotionModifierActivate(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier);

	UFUNCTION()
	void OnRootMotionModifierUpdate(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier);

	UFUNCTION()
	void OnRootMotionModifierDeactivate(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier);

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	void OnWarpBegin(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	void OnWarpUpdate(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Warping")
	void OnWarpEnd(UMotionWarpingComponent* MotionWarpingComp, URootMotionModifier* Modifier) const;

#if WITH_EDITOR
	virtual void ValidateAssociatedAssets() override;
#endif
};