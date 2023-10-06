// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

class UClass;
class USkeletalMeshComponent;

class UAnimSharingTransitionInstance;

struct ANIMATIONSHARING_API FTransitionBlendInstance
{
public:	
	FTransitionBlendInstance();
	void Initialise(USkeletalMeshComponent* InSkeletalMeshComponent, UClass* InAnimationBP);
	void Setup(USkeletalMeshComponent* InFromComponent, USkeletalMeshComponent* InToComponent, float InBlendTime);
	void Stop();

	USkeletalMeshComponent* GetComponent() const;
	USkeletalMeshComponent* GetToComponent() const;
	USkeletalMeshComponent* GetFromComponent() const;

protected:
	USkeletalMeshComponent * SkeletalMeshComponent;
	UAnimSharingTransitionInstance* TransitionInstance;
	USkeletalMeshComponent* FromComponent;
	USkeletalMeshComponent* ToComponent;
	float BlendTime;
	bool bBlendState;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#endif
