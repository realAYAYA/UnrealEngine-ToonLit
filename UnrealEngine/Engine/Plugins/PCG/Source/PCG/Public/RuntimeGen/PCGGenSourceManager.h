// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RuntimeGen/GenSources/PCGGenSourceBase.h"

class APCGWorldActor;
class UPCGGenSourceComponent;
class UPCGGenSourceEditorCamera;
class UPCGGenSourceWPStreamingSource;

class AController;
class AGameModeBase;
class APlayerController;
class UWorld;

/**
 * The runtime Generation Source Manager tracks generation sources in the world for use by the Runtime Generation Scheduler.
 */
class FPCGGenSourceManager
{
public:
	FPCGGenSourceManager(const UWorld* InWorld);
	~FPCGGenSourceManager();

	/** Marks the GenSourceManager as dirty so that the next call to 'GetAllGenSources()' will update tracked generation sources. */
	void Tick() { bDirty = true; }

	/** Creates the set of all generation sources tracked by the manager. Call 'Tick()' to keep tracked generation sources up to date. */
	TSet<IPCGGenSourceBase*> GetAllGenSources(const APCGWorldActor* InPCGWorldActor);

	/** Adds a UPCGGenSource to be tracked by the GenSourceManager. */
	bool RegisterGenSource(IPCGGenSourceBase* InGenSource);

	/** Removes a UPCGGenSource from being tracked by the GenSourceManager. */
	bool UnregisterGenSource(const IPCGGenSourceBase* InGenSource);

protected:
	void OnGameModePostLogin(AGameModeBase* InGameMode, APlayerController* InPlayerController);
	void OnGameModePostLogout(AGameModeBase* InGameMode, AController* InController);

	/** Updates tracked generation sources that should be refreshed per tick. */
	void UpdatePerTickGenSources(const APCGWorldActor* InPCGWorldActor);

protected:
	/** Tracks registered generation sources, such as UPCGGenSourceComponent and UPCGGenSourcePlayer. */
	TSet<IPCGGenSourceBase*> RegisteredGenSources;

#if WITH_EDITORONLY_DATA
	/** Tracks the active/main editor viewport client. This is refreshed every tick to keep a handle to whichever viewport is active. */
	UPCGGenSourceEditorCamera* EditorCameraGenSource = nullptr;
#endif

	/** Pool of GenSources dedicated to WorldPartition StreamingSources. This list grows as necessary and refreshes every tick. */
	TArray<UPCGGenSourceWPStreamingSource*> WorldPartitionGenSources;

	/** There may be more generation sources in 'WorldPartitionGenSources' than there are streaming sources in the world, so we only use the first N generation sources. */
	int32 NumWorldPartitionStreamingSources = 0;

	const UWorld* World = nullptr;
	bool bDirty = false;
};
