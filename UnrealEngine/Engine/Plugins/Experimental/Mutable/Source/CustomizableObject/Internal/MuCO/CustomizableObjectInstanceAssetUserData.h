// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "GameplayTagContainer.h"

#include "CustomizableObjectInstanceAssetUserData.generated.h"


UCLASS(BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableObjectInstanceUserData : public UAssetUserData
{
	GENERATED_BODY()

public:

	/** Return the list of tags for this instance. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	const FGameplayTagContainer& GetAnimationGameplayTags() const
	{
		return AnimBPGameplayTags;
	};

	/** Sets the list of tags for this instance. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetAnimationGameplayTags(const FGameplayTagContainer& InstanceTags)
	{
		AnimBPGameplayTags = InstanceTags;
	};

private:

	UPROPERTY()
	FGameplayTagContainer AnimBPGameplayTags;
};
