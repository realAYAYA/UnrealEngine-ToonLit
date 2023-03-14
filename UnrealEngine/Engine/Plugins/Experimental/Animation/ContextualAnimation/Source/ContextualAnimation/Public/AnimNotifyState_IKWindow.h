// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AlphaBlend.h"
#include "AnimNotifyState_IKWindow.generated.h"

/** AnimNotifyState used to define areas in an animation where IK should be enable */
UCLASS(meta = (DisplayName = "IK Window"))
class CONTEXTUALANIMATION_API UAnimNotifyState_IKWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Config)
	FName GoalName;

	UPROPERTY(EditAnywhere, Category = Config)
	FAlphaBlend BlendIn;

	UPROPERTY(EditAnywhere, Category = Config)
	FAlphaBlend BlendOut;

	UPROPERTY()
	bool bEnable = true;

	UAnimNotifyState_IKWindow(const FObjectInitializer& ObjectInitializer);

	virtual FString GetNotifyName_Implementation() const override;

	static float GetIKAlphaValue(const FName& GoalName, const struct FAnimMontageInstance* MontageInstance);
};