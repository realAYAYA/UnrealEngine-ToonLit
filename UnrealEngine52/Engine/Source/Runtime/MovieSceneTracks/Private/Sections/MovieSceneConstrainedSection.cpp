// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneConstrainedSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneConstrainedSection)


IMovieSceneConstrainedSection::IMovieSceneConstrainedSection() : bDoNotRemoveChannel(false)
{
}


void IMovieSceneConstrainedSection::SetDoNoRemoveChannel(bool bInDoNotRemoveChannel)
{
	bDoNotRemoveChannel = bInDoNotRemoveChannel;
}


