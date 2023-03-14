// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AnimationSharingTypes.h"

#include "AnimationSharingSetup.generated.h"

/**
 * The Animation Sharing Setup asset contains all the information that will be shared across the specified Actors
 */
UCLASS(hidecategories = Object, Blueprintable, config = Engine)
class ANIMATIONSHARING_API UAnimationSharingSetup : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(EditAnywhere, config, Category = AnimationSharing)
	TArray<FPerSkeletonAnimationSharingSetup> SkeletonSetups;

	UPROPERTY(EditAnywhere, config, Category = AnimationSharing)
	FAnimationSharingScalability ScalabilitySettings;
};