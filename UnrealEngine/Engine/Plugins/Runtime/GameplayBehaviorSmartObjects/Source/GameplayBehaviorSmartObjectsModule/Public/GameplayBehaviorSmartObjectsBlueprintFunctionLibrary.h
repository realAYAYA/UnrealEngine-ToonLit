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
	UE_DEPRECATED(5.3, "Use MoveToAndUseSmartObjectWithGameplayBehavior or UseSmartObjectWithGameplayBehavior using a claim handle from UAITask_UseGameplayBehaviorSmartObject instead.")
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeprecatedFunction, DeprecationMessage = "Use MoveToAndUseSmartObjectWithGameplayBehavior or UseSmartObjectWithGameplayBehavior using a claim handle from UAITask_UseGameplayBehaviorSmartObject instead."))
	static bool UseGameplayBehaviorSmartObject(AActor* Avatar, AActor* SmartObject);
};