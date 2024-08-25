// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpawnHelper.h"

#include "Commands/TestCommandBuilder.h"

#define ENABLE_MAPSPAWNER_TEST WITH_EDITOR && WITH_AUTOMATION_TESTS

#if ENABLE_MAPSPAWNER_TEST
#include "UnrealEdMisc.h"

/// Class for spawning Actors in a named Map/Level
struct CQTEST_API FMapTestSpawner : public FSpawnHelper
{
	/**
	 * Construct the MapTestSpawner.
	 *
	 * @param MapDirectory - The directory which the map resides in.
	 * @param MapName - Name of the map.
	 */
	FMapTestSpawner(const FString& MapDirectory, const FString& MapName);

	/**
	 * Creates an instance of the MapTestSpawner with a temporary level ready for use.
	 * 
	 * @param InCommandBuilder - Test Command Builder used to assist with setup.
	 * @return unique instance of the FMapTestSpawner, nullptr otherwise
	 */
	static TUniquePtr<FMapTestSpawner> CreateFromTempLevel(FTestCommandBuilder& InCommandBuilder);

	/**
	 * Loads the map specified from the MapDirectory and MapName to be prepared for the test.
	 *
	 * @param TestRunner - TestRunner used to send the latent command needed for map preparations.
	 */
	void AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner);

	/**
	 * Finds the first pawn in the given map.
	 */
	APawn* FindFirstPlayerPawn();

protected:
    virtual UWorld* CreateWorld() override;

private:
	/**
	 * Handler called on map changed.
	 */
	void OnMapChanged(UWorld* World, EMapChangeType ChangeType);

	FString MapDirectory;
	FString MapName;
	UWorld* PieWorld{ nullptr };
	FDelegateHandle MapChangedHandle;
};

#endif // ENABLE_MAPSPAWNER_TEST