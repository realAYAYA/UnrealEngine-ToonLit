// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCinePrestreamingSection.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "TrackInstances/MovieSceneCinePrestreamingTrackInstance.h"
#include "EntitySystem/MovieSceneEntityBuilder.h"

UMovieSceneCinePrestreamingSection::UMovieSceneCinePrestreamingSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}

void UMovieSceneCinePrestreamingSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	FMovieSceneTrackInstanceComponent TrackInstance{ decltype(FMovieSceneTrackInstanceComponent::Owner)(this), UMovieSceneCinePrestreamingTrackInstance::StaticClass() };

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.AddTag(FBuiltInComponentTypes::Get()->Tags.Root)
		.Add(FBuiltInComponentTypes::Get()->TrackInstance, TrackInstance)
	);
}
