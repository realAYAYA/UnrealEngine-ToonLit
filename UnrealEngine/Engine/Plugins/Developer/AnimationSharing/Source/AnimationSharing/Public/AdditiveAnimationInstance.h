// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"

class UAnimSharingAdditiveInstance;
class UAnimSequence;

struct ANIMATIONSHARING_API FAdditiveAnimationInstance
{
public:
	FAdditiveAnimationInstance();

	void Initialise(USkeletalMeshComponent* InSkeletalMeshComponent, UClass* InAnimationBP);
	void Setup(USkeletalMeshComponent* InBaseComponent, UAnimSequence* InAnimSequence);
	void UpdateBaseComponent(USkeletalMeshComponent* InBaseComponent);
	void Stop();
	void Start();

	USkeletalMeshComponent* GetComponent() const;
	USkeletalMeshComponent* GetBaseComponent() const;	

protected:
	USkeletalMeshComponent * SkeletalMeshComponent;
	UAnimSharingAdditiveInstance* AdditiveInstance;
	UAnimSequence* AdditiveAnimationSequence;
	USkeletalMeshComponent* BaseComponent;
	bool bLoopingState;
};