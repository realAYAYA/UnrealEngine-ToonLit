// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieScenePrimitiveMaterialTrack)


UMovieScenePrimitiveMaterialTrack::UMovieScenePrimitiveMaterialTrack(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	MaterialIndex = 0;
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(64,192,64,75);
#endif
}

UMovieSceneSection* UMovieScenePrimitiveMaterialTrack::CreateNewSection()
{
	return NewObject<UMovieScenePrimitiveMaterialSection>(this, NAME_None, RF_Transactional);
}

bool UMovieScenePrimitiveMaterialTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieScenePrimitiveMaterialSection::StaticClass();
}

int32 UMovieScenePrimitiveMaterialTrack::GetMaterialIndex() const
{
	return MaterialIndex;
}

void UMovieScenePrimitiveMaterialTrack::SetMaterialIndex(int32 InMaterialIndex)
{
	MaterialIndex = InMaterialIndex;
}
