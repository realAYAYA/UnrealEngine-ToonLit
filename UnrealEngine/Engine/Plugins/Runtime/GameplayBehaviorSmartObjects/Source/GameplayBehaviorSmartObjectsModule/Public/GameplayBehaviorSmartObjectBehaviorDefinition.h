// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDefinition.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.generated.h"

class UGameplayBehaviorConfig;

/**
 * SmartObject behavior definition for the GameplayBehavior framework
 */
UCLASS()
class GAMEPLAYBEHAVIORSMARTOBJECTSMODULE_API UGameplayBehaviorSmartObjectBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = SmartObject, Instanced)
	TObjectPtr<UGameplayBehaviorConfig> GameplayBehaviorConfig;
};