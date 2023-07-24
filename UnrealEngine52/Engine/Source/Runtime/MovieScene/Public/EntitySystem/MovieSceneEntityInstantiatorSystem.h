// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "MovieSceneInstanceRegistry.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneEntityInstantiatorSystem.generated.h"

class UMovieSceneEntitySystem;
class UMovieSceneEntitySystemLinker;
class UObject;
namespace UE { namespace MovieScene { template <typename T> struct TComponentTypeID; } }
struct FGuid;
struct FMovieSceneObjectBindingID;

UCLASS(Abstract)
class MOVIESCENE_API UMovieSceneEntityInstantiatorSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UMovieSceneEntityInstantiatorSystem(const FObjectInitializer& ObjInit);

	void UnlinkStaleObjectBindings(UE::MovieScene::TComponentTypeID<FGuid> BindingType);
	void UnlinkStaleObjectBindings(UE::MovieScene::TComponentTypeID<FMovieSceneObjectBindingID> BindingType);
};
