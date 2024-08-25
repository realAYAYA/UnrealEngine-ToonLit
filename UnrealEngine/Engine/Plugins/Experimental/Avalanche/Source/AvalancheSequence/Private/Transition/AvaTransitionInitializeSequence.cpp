// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionInitializeSequence.h"
#include "AvaSequencePlaybackObject.h"
#include "Math/NumericLimits.h"

TArray<UAvaSequencePlayer*> FAvaTransitionInitializeSequence::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

	FAvaSequencePlayParams PlaySettings;

	// Set start to largest double, so that it gets clamped down to be at the End Time (i.e. time that the Sequence should evaluate)
	PlaySettings.Start = FAvaSequenceTime(TNumericLimits<double>::Max());
	PlaySettings.End = InitializeTime;
	PlaySettings.PlayMode = PlayMode;

	switch (QueryType)
	{
	case EAvaTransitionSequenceQueryType::Name:
		return PlaybackObject->PlaySequencesByLabel(SequenceName, PlaySettings);

	case EAvaTransitionSequenceQueryType::Tag:
		if (const FAvaTag* Tag = SequenceTag.GetTag())
		{
			return PlaybackObject->PlaySequencesByTag(*Tag, bPerformExactMatch, PlaySettings);
		}
		return TArray<UAvaSequencePlayer*>();
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
