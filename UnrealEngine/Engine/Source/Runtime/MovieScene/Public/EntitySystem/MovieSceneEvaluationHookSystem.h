// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/IMovieSceneEvaluationHook.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/FrameTime.h"
#include "MovieSceneSequenceID.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneEvaluationHookSystem.generated.h"

class IMovieScenePlayer;
class UMovieSceneEntitySystemLinker;
class UObject;


USTRUCT()
struct FMovieSceneEvaluationHookEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieSceneEvaluationHookComponent Hook;

	FMovieSceneSequenceID SequenceID;

	int32 TriggerIndex;

	FFrameTime RootTime;

	UE::MovieScene::EEvaluationHookEvent Type;

	bool bRestoreState = false;
};

USTRUCT()
struct FMovieSceneEvaluationHookEventContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMovieSceneEvaluationHookEvent> Events;
};

USTRUCT()
struct FMovieSceneEvaluationInstanceKey
{
	GENERATED_BODY()

	FMovieSceneEvaluationInstanceKey()
	{}

	FMovieSceneEvaluationInstanceKey(UE::MovieScene::FInstanceHandle InInstanceHandle)
		: InstanceHandle(InInstanceHandle)
	{}

	friend uint32 GetTypeHash(FMovieSceneEvaluationInstanceKey InKey)
	{
		return GetTypeHash(InKey.InstanceHandle);
	}
	friend bool operator==(FMovieSceneEvaluationInstanceKey A, FMovieSceneEvaluationInstanceKey B)
	{
		return A.InstanceHandle == B.InstanceHandle;
	}

	UE::MovieScene::FInstanceHandle InstanceHandle;
};


UCLASS()
class MOVIESCENE_API UMovieSceneEvaluationHookSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UMovieSceneEvaluationHookSystem(const FObjectInitializer& ObjInit);

	void AddEvent(UE::MovieScene::FInstanceHandle RootInstance, const FMovieSceneEvaluationHookEvent& InEvent);

	void SortEvents();

protected:

	bool HasEvents() const;
	void TriggerAllEvents();

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;

	void UpdateHooks();

	UPROPERTY()
	TMap<FMovieSceneEvaluationInstanceKey, FMovieSceneEvaluationHookEventContainer> PendingEventsByRootInstance;
};