// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaSection.h"
#include "MovieScene.h"
#include "Misc/FrameRate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneMediaSection)

#define LOCTEXT_NAMESPACE "MovieSceneMediaSection"

namespace
{
	FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame)
	{
		return StartOffset + TrimTime.Time.FrameNumber - StartFrame;
	}
}

/* UMovieSceneMediaSection interface
 *****************************************************************************/

UMovieSceneMediaSection::UMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bLooping(true)
	, bHasMediaPlayerProxy(false)
{
#if WITH_EDITORONLY_DATA
	ThumbnailReferenceOffset = 0.f;
#endif

	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
}

void UMovieSceneMediaSection::PostInitProperties()
{
	Super::PostInitProperties();

	UMovieScene* Outer = GetTypedOuter<UMovieScene>();
	FFrameRate TickResolution = Outer ? Outer->GetTickResolution() : FFrameRate(24, 1);

	// media tracks have some preroll by default to precache frames
	SetPreRollFrames( (0.5 * TickResolution).RoundToFrame().Value );
}

void UMovieSceneMediaSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	if (TryModify())
	{
		if (bTrimLeft)
		{
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
	}
}

UMovieSceneSection* UMovieSceneMediaSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	const FFrameNumber InitialStartFrameOffset = StartFrameOffset;

	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
	if (NewSection != nullptr)
	{
		UMovieSceneMediaSection* NewMediaSection = Cast<UMovieSceneMediaSection>(NewSection);
		NewMediaSection->StartFrameOffset = NewOffset;
	}

	// Restore original offset modified by splitting
	StartFrameOffset = InitialStartFrameOffset;

	return NewSection;
}

TOptional<FFrameTime> UMovieSceneMediaSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(StartFrameOffset);
}

void UMovieSceneMediaSection::MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	if (StartFrameOffset.Value > 0)
	{
		FFrameNumber NewStartFrameOffset = ConvertFrameTime(FFrameTime(StartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		StartFrameOffset = NewStartFrameOffset;
	}
}

#undef LOCTEXT_NAMESPACE

