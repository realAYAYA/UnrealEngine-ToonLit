// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDefinition.h"
#include "StateTreeReference.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.generated.h"

/**
 * SmartObject behavior definition for the GameplayInteractions
 */
UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UGameplayInteractionSmartObjectBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="", meta=(Schema="/Script/GameplayInteractionsModule.GameplayInteractionStateTreeSchema"))
	FStateTreeReference StateTreeReference;
};
