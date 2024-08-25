// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionPlaySequenceTask.h"
#include "AvaSequence.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequencePlayer.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

#define LOCTEXT_NAMESPACE "AvaTransitionPlaySequenceTask"

FText FAvaTransitionPlaySequenceTask::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	FText DescriptionFormat = LOCTEXT("TaskDescription", "Play {SequenceQueryText} {AdditionalArgs}");

	FFormatNamedArguments NamedArgs;
	NamedArgs.Add(TEXT("SequenceQueryText"), GetSequenceQueryText());

	TArray<FText> AdditionalArgs;
	if (PlaySettings.PlayMode != EAvaSequencePlayMode::Forward)
	{
		AdditionalArgs.Add(UEnum::GetDisplayValueAsText(PlaySettings.PlayMode));
	}
	AdditionalArgs.Add(UEnum::GetDisplayValueAsText(WaitType));

	if (!AdditionalArgs.IsEmpty())
	{
		NamedArgs.Add(TEXT("AdditionalArgs"), FText::Format(INVTEXT("( {0} )"), FText::Join(FText::FromString(TEXT(" | ")), AdditionalArgs)));	
	}

	return FText::Format(DescriptionFormat, NamedArgs);
}

TArray<UAvaSequencePlayer*> FAvaTransitionPlaySequenceTask::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

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

#undef LOCTEXT_NAMESPACE
