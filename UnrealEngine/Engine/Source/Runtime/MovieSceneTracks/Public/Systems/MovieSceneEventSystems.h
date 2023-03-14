// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "Channels/MovieSceneEvent.h"
#include "MovieSceneEventSystems.generated.h"

class IMovieScenePlayer;

USTRUCT()
struct FMovieSceneEventTriggerData
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieSceneEventPtrs Ptrs;

	UPROPERTY()
	FGuid ObjectBindingID;

	FMovieSceneSequenceID SequenceID;

	FFrameTime RootTime;
};

/**
 * Systems that triggers events based on one-shot FMovieSceneEventComponent components
 * Works by iterating all pending instances of TMovieSceneComponentID<FMovieSceneEventComponent> and triggering inline.
 * Does not dispatch any async tasks
 */

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneEventSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UMovieSceneEventSystem(const FObjectInitializer& ObjInit);

	void AddEvent(UE::MovieScene::FInstanceHandle RootInstance, const FMovieSceneEventTriggerData& TriggerData);

protected:

	bool HasEvents() const;
	void TriggerAllEvents();

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnUnlink() override final;

	static void TriggerEvents(TArrayView<const FMovieSceneEventTriggerData> Events, IMovieScenePlayer* Player);
	static void TriggerEventWithParameters(UObject* DirectorInstance, const FMovieSceneEventTriggerData& Event, TArrayView<UObject* const> GlobalContexts, IMovieScenePlayer* Player);
	static bool PatchBoundObject(uint8* Parameters, UObject* BoundObject, FProperty* BoundObjectProperty, IMovieScenePlayer* Player, FMovieSceneSequenceID SequenceID);

	/** Events grouped by root sequence instance handle */
	TMap< UE::MovieScene::FInstanceHandle, TArray<FMovieSceneEventTriggerData> > EventsByRoot;
};

/** System that triggers events before any spawnables */
UCLASS()
class MOVIESCENETRACKS_API UMovieScenePreSpawnEventSystem : public UMovieSceneEventSystem
{
public:
	GENERATED_BODY()

	UMovieScenePreSpawnEventSystem(const FObjectInitializer& ObjInit);
};


/** System that triggers events after any spawnables */
UCLASS()
class MOVIESCENETRACKS_API UMovieScenePostSpawnEventSystem : public UMovieSceneEventSystem
{
public:
	GENERATED_BODY()

	UMovieScenePostSpawnEventSystem(const FObjectInitializer& ObjInit);
};

/** System that triggers events right at the end of evaluation */
UCLASS()
class MOVIESCENETRACKS_API UMovieScenePostEvalEventSystem : public UMovieSceneEventSystem
{
public:
	GENERATED_BODY()

	UMovieScenePostEvalEventSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)override;
};
