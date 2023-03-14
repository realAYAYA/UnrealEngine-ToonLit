// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "Math/Transform.h"

#include "MovieSceneTransformOriginSystem.generated.h"

struct FMovieSceneAnimTypeID;

UCLASS(MinimalAPI)
class UMovieSceneTransformOriginSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneTransformOriginSystem(const FObjectInitializer& ObjInit);

private:

	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	TSparseArray<FTransform> TransformOriginsByInstanceID;
};



