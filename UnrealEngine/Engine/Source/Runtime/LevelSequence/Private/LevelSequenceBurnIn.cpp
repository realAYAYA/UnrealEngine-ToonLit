// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceBurnIn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceBurnIn)

ULevelSequenceBurnIn::ULevelSequenceBurnIn( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

void ULevelSequenceBurnIn::TakeSnapshotsFrom(ALevelSequenceActor& InActor)
{
	LevelSequenceActor = &InActor;
	if (ensure(InActor.SequencePlayer))
	{
		InActor.SequencePlayer->OnSequenceUpdated().AddUObject(this, &ULevelSequenceBurnIn::OnSequenceUpdated);
		InActor.SequencePlayer->TakeFrameSnapshot(FrameInformation);
	}
}

void ULevelSequenceBurnIn::OnSequenceUpdated(const UMovieSceneSequencePlayer& Player, FFrameTime CurrentTime, FFrameTime PreviousTime)
{
	static_cast<const ULevelSequencePlayer&>(Player).TakeFrameSnapshot(FrameInformation);
}

