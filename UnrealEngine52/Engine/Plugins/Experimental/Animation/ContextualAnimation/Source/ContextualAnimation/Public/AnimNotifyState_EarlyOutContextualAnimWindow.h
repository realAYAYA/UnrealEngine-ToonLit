// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EarlyOutContextualAnimWindow.generated.h"

/** Notify used to allow player to early out from a contextual anim. Usually used at the end of the animations to improve responsivess */
UCLASS(meta = (DisplayName = "Early Out Contextual Anim"))
class CONTEXTUALANIMATION_API UAnimNotifyState_EarlyOutContextualAnimWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UAnimNotifyState_EarlyOutContextualAnimWindow(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
