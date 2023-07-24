// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "Misc/FrameTime.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneEvalTimeSystem.generated.h"

class UObject;
struct FFrameTime;

UCLASS()
class MOVIESCENE_API UMovieSceneEvalTimeSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UMovieSceneEvalTimeSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;

private:
	struct FEvaluatedTime
	{
		FFrameTime FrameTime;
		double Seconds;
	};
	TArray<FEvaluatedTime> EvaluatedTimes;

	UE::MovieScene::FEntityComponentFilter RelevantFilter;
};

