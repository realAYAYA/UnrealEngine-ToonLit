// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"

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