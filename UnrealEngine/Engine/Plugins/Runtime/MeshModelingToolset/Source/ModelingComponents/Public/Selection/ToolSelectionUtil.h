// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UInteractiveToolManager;
class AActor;

/**
 * Utility functions for Tool implementations to use when doing selection
 */
namespace ToolSelectionUtil
{

	/**
	 * Change the active selection to the given Actor, via given ToolManager. Replaces existing selection.
	 */
	MODELINGCOMPONENTS_API void SetNewActorSelection(UInteractiveToolManager* ToolManager, AActor* Actor);

	/**
	 * Change the active selection to the given Actors, via given ToolManager. Replaces existing selection.
	 */
	MODELINGCOMPONENTS_API void SetNewActorSelection(UInteractiveToolManager* ToolManager, const TArray<AActor*>& Actors);
}