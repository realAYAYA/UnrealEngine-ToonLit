// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceActorSpawner.h"

/** Chaos cache Manager spawner to create a new cache manager from the spawnable template */
class CHAOSCACHING_API FChaosCacheObjectSpawner : public FLevelSequenceActorSpawner
{
public:
	FChaosCacheObjectSpawner();

	/** Static method to create the object spawner */
	static TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	// IMovieSceneObjectSpawner interface
	virtual UClass* GetSupportedTemplateType() const override;
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
	virtual void DestroySpawnedObject(UObject& Object) override;
	virtual bool IsEditor() const override { return true; }
	virtual int32 GetSpawnerPriority() const override { return 1; }
};