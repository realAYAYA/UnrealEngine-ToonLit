// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructView.h"
#include "UObject/Interface.h"
#include "GameplayActuationStateProvider.generated.h"

/** 
 * Interface for GameplayTasks to return Actuation state.
 * This is used by the GameplayActuationComponent to keep track of the active actuation state based on active tasks.
 */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UGameplayActuationStateProvider : public UInterface
{
	GENERATED_BODY()
};

class IGameplayActuationStateProvider
{
	GENERATED_BODY()
public:
	/** @return view to the actuation state of the gameplay task. */
	virtual FConstStructView GetActuationState() const { return FConstStructView(); }
};
