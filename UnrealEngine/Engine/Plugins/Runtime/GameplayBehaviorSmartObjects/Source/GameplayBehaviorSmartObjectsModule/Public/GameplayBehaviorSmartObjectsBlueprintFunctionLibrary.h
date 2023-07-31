// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayBehaviorSmartObjectsBlueprintFunctionLibrary.generated.h"

class AActor;

UCLASS(meta = (ScriptName = "GameplayBehaviorsLibrary"))
class GAMEPLAYBEHAVIORSMARTOBJECTSMODULE_API UGameplayBehaviorSmartObjectsBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DisplayName = "UseGameplayBehaviorSmartObject"))
	static bool UseGameplayBehaviorSmartObject(AActor* Avatar, AActor* SmartObject);
};