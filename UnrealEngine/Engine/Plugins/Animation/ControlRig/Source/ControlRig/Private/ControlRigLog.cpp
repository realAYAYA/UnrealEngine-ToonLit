// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigLog.h"

void FControlRigLog::Reset()
{
#if WITH_EDITOR
	Entries.Reset();
	KnownMessages.Reset();
#endif
}

void FControlRigLog::Report(EMessageSeverity::Type InSeverity, const FName& InOperatorName, int32 InInstructionIndex, const FString& InMessage)
{
#if WITH_EDITOR

	if (!InMessage.IsEmpty())
	{
		if (KnownMessages.Contains(InMessage))
		{
			return;
		}
	}

	int32 EntryIndex = Entries.Add(FLogEntry(InSeverity, InOperatorName, InInstructionIndex, InMessage));
	KnownMessages.Add(InMessage, true);
#endif
}
