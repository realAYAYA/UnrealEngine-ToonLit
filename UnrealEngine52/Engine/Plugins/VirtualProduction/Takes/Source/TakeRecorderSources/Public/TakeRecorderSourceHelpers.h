// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "TakeRecorderSources.h"
#include "GameFramework/Actor.h"

class AActor;

namespace TakeRecorderSourceHelpers
{
	/**
	 * Adds a number of actors to the set of sources to record from.
	 * 
	 * @param TakeRecorderSources The list of sources used for the current take. 
	 * @param ActorsToRecord The list of Actors that should be added to Sources. Note that this can include ALevelSequenceActors.
	 * @param bReduceKeys Enable/disable key reduction on all the sources registered
	 * @param bShowProgress Enable/disable the dialog box showing progress for the potentially slow parts of finalizing the take
	 */
	TAKERECORDERSOURCES_API void AddActorSources(
		UTakeRecorderSources*     TakeRecorderSources,
		TArrayView<AActor* const> ActorsToRecord,
		bool                      bReduceKeys = true,
		bool                      bShowProgress = true);

	/**
	 * Removes all sources from a list of sources to record from.
	 */
	TAKERECORDERSOURCES_API void RemoveAllActorSources(UTakeRecorderSources* Sources);
};
