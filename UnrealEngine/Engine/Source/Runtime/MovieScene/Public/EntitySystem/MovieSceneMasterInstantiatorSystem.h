// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneMasterInstantiatorSystem.generated.h"

class UObject;
namespace UE { namespace MovieScene { struct FEntityAllocation; } }


UCLASS()
class MOVIESCENE_API UMovieSceneMasterInstantiatorSystem : public UMovieSceneEntityInstantiatorSystem
{
	GENERATED_BODY()

public:

	UMovieSceneMasterInstantiatorSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	void InstantiateAllocation(const UE::MovieScene::FEntityAllocation* ParentAllocation);
};