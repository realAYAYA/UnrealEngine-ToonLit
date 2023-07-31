// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneSpawnRegister.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"

class IMovieSceneObjectSpawner;
class IMovieScenePlayer;
class UClass;
class UObject;
struct FMovieSceneSpawnable;

/** Movie scene spawn register that knows how to handle spawning objects (actors) for a level sequence  */
class LEVELSEQUENCE_API FLevelSequenceSpawnRegister : public FMovieSceneSpawnRegister
{
public:
	FLevelSequenceSpawnRegister();

protected:
	/** ~ FMovieSceneSpawnRegister interface */
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
	virtual void DestroySpawnedObject(UObject& Object) override;

#if WITH_EDITOR
	virtual bool CanSpawnObject(UClass* InClass) const override;
#endif

protected:
	/** Extension object spawners */
	TArray<TSharedRef<IMovieSceneObjectSpawner>> MovieSceneObjectSpawners;
};
