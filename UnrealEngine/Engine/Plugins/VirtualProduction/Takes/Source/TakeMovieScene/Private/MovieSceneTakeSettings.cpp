// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTakeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneTakeSettings)

#define LOCTEXT_NAMESPACE "MovieSceneTakeSettings"

UMovieSceneTakeSettings::UMovieSceneTakeSettings()
	: HoursName(TEXT("Hours"))
	, MinutesName(TEXT("Minutes"))
	, SecondsName(TEXT("Seconds"))
	, FramesName(TEXT("Frames"))
	, SubFramesName(TEXT("SubFrames"))
	, SlateName(TEXT("Slate"))
{
}

#if WITH_EDITOR
void UMovieSceneTakeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		// Dragging spinboxes causes this to be called every frame so we wait until they've finished dragging before saving.
		SaveConfig();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
