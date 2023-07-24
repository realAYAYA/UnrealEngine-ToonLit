// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCameraShakeSourceTriggerSection.h"
#include "Channels/MovieSceneChannelProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneCameraShakeSourceTriggerSection)

UMovieSceneCameraShakeSourceTriggerSection::UMovieSceneCameraShakeSourceTriggerSection(const FObjectInitializer& Init)
	: Super(Init)
{
#if WITH_EDITOR
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Channel, FMovieSceneChannelMetaData());
#else
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Channel);
#endif
}


