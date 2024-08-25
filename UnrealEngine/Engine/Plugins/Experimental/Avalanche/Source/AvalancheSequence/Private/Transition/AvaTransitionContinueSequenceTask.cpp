// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionContinueSequenceTask.h"
#include "AvaSequencePlaybackObject.h"
#include "StateTreeExecutionContext.h"

TArray<UAvaSequencePlayer*> FAvaTransitionContinueSequenceTask::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

	switch (QueryType)
	{
	case EAvaTransitionSequenceQueryType::Name:
		return PlaybackObject->ContinueSequencesByLabel(SequenceName);

	case EAvaTransitionSequenceQueryType::Tag:
		if (const FAvaTag* Tag = SequenceTag.GetTag())
		{
			return PlaybackObject->ContinueSequencesByTag(*Tag, bPerformExactMatch);
		}
		return TArray<UAvaSequencePlayer*>();
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
