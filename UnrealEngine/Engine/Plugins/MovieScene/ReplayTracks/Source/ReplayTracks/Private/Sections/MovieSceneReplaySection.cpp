// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneReplaySection.h"
#include "Systems/MovieSceneReplaySystem.h"

UMovieSceneReplaySection::UMovieSceneReplaySection(const FObjectInitializer& ObjInitializer)
	: Super(ObjInitializer)
{
}

void UMovieSceneReplaySection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;
	
	const FReplayComponentTypes* ReplayComponents = FReplayComponentTypes::Get();
	
	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.Add(ReplayComponents->Replay, this)
		);
}
