// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneVectorTrack.h"
#include "Sections/MovieSceneVectorSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneVectorTrack)


UMovieSceneFloatVectorTrack::UMovieSceneFloatVectorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	NumChannelsUsed = 0;
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


bool UMovieSceneFloatVectorTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneFloatVectorSection::StaticClass();
}

UMovieSceneSection* UMovieSceneFloatVectorTrack::CreateNewSection()
{
	UMovieSceneFloatVectorSection* NewSection = NewObject<UMovieSceneFloatVectorSection>(this, NAME_None, RF_Transactional);
	NewSection->SetChannelsUsed(NumChannelsUsed);
	return NewSection;
}


UMovieSceneDoubleVectorTrack::UMovieSceneDoubleVectorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	NumChannelsUsed = 0;
	SupportedBlendTypes = FMovieSceneBlendTypeField::All();
}


bool UMovieSceneDoubleVectorTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneDoubleVectorSection::StaticClass();
}

UMovieSceneSection* UMovieSceneDoubleVectorTrack::CreateNewSection()
{
	UMovieSceneDoubleVectorSection* NewSection = NewObject<UMovieSceneDoubleVectorSection>(this, NAME_None, RF_Transactional);
	NewSection->SetChannelsUsed(NumChannelsUsed);
	return NewSection;
}

