// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceActorSpawner.h"

/** Chaos cache Manager spawner to create a new cache manager from the spawnable template */
class FChaosCacheObjectSpawner : public FLevelSequenceActorSpawner
{
public:
	CHAOSCACHING_API FChaosCacheObjectSpawner();

	/** Static method to create the object spawner */
	static CHAOSCACHING_API TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	// IMovieSceneObjectSpawner interface
	CHAOSCACHING_API virtual UClass* GetSupportedTemplateType() const override;
	CHAOSCACHING_API virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) override;
	CHAOSCACHING_API virtual void DestroySpawnedObject(UObject& Object) override;
	virtual bool IsEditor() const override { return true; }
	virtual int32 GetSpawnerPriority() const override { return 1; }
};
