// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "HAL/Platform.h"
#include "Serialization/Archive.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneTrackInstanceSystem.generated.h"

class UClass;
class UMovieSceneSection;
class UMovieSceneTrackInstance;
class UObject;

USTRUCT()
struct FMovieSceneTrackInstanceEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UObject> BoundObject = nullptr;

	UPROPERTY()
	TObjectPtr<UMovieSceneTrackInstance> TrackInstance = nullptr;
};


UCLASS()
class MOVIESCENE_API UMovieSceneTrackInstanceInstantiator : public UMovieSceneEntityInstantiatorSystem
{
	GENERATED_BODY()

public:

	UMovieSceneTrackInstanceInstantiator(const FObjectInitializer& ObjInit);

	int32 MakeOutput(UObject* BoundObject, UClass* TrackInstanceClass);

	int32 FindOutput(UObject* BoundObject, UClass* TrackInstanceClass) const;

	const TSparseArray<FMovieSceneTrackInstanceEntry>& GetTrackInstances() const
	{
		return TrackInstances;
	}

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:

	virtual void OnLink() override final;
	virtual void OnUnlink() override final;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	virtual void OnTagGarbage() override;

	virtual void Serialize(FArchive& Ar) override;

	TSparseArray<FMovieSceneTrackInstanceEntry> TrackInstances;
	TMultiMap<UObject*, int32> BoundObjectToInstances;

	TBitArray<> InvalidatedOutputs;

	int32 ChildInitializerIndex;
};


UCLASS()
class MOVIESCENE_API UMovieSceneTrackInstanceSystem : public UMovieSceneEntitySystem
{
	GENERATED_BODY()

	UMovieSceneTrackInstanceSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnLink() override final;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	UPROPERTY()
	TObjectPtr<UMovieSceneTrackInstanceInstantiator> Instantiator;
};


