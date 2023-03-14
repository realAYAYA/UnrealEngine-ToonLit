// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "MovieSceneSpawnRegister.h"

/**
 * Implements the LevelSequence module.
 */
class ILevelSequenceModule : public IModuleInterface
{
public:

	/** Register an object spawner */
	virtual FDelegateHandle RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner InOnCreateMovieSceneObjectSpawner) = 0;

	/** Populate the specified array with new object spawner instances for all registered object spawner types */
	virtual void GenerateObjectSpawners(TArray<TSharedRef<IMovieSceneObjectSpawner>>& OutSpawners) const = 0;

	/** Unregister an object spawner */
	virtual void UnregisterObjectSpawner(FDelegateHandle InHandle) = 0;
};
