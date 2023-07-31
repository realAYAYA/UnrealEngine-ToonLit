// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderActorGroup.h"
#include "SequenceRecorder.h"
#include "ActorRecording.h"

void USequenceRecorderActorGroup::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderActorGroup, SequenceName))
		{
			GroupName = *SequenceName;
			TargetLevelSequence = nullptr;

			for (UActorRecording* ActorRecording : RecordedActors)
			{
				if (ActorRecording != nullptr)
				{
					ActorRecording->TakeNumber = 1;
				}
			}
		}

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderActorGroup, SequenceName) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderActorGroup, SequenceRecordingBasePath))
		{
			FSequenceRecorder::Get().ForceRefreshNextSequence();
		}
	}
}