// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTrackEvaluationField.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneTrackEvaluationField)


FMovieSceneTrackEvaluationData FMovieSceneTrackEvaluationData::FromSection(UMovieSceneSection* InSection)
{
	FMovieSceneTrackEvaluationData NewData;
	NewData.Section = InSection;
	return NewData;
}

FMovieSceneTrackEvaluationData FMovieSceneTrackEvaluationData::FromTrack(UMovieSceneTrack* InTrack)
{
	FMovieSceneTrackEvaluationData NewData;
	NewData.Track = InTrack;
	return NewData;
}
