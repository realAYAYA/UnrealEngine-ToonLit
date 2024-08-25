// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionWaitForSequenceTask.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequencePlayer.h"
#include "StateTreeExecutionContext.h"

TArray<UAvaSequencePlayer*> FAvaTransitionWaitForSequenceTask::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

	switch (QueryType)
	{
	case EAvaTransitionSequenceQueryType::Name:
		return PlaybackObject->GetSequencePlayersByLabel(SequenceName);

	case EAvaTransitionSequenceQueryType::Tag:
		if (const FAvaTag* Tag = SequenceTag.GetTag())
		{
			return PlaybackObject->GetSequencePlayersByTag(*Tag, bPerformExactMatch);
		}
		return TArray<UAvaSequencePlayer*>();
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
