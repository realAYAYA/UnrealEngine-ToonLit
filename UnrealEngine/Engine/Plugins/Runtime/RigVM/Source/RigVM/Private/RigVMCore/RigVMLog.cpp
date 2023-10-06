// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMLog.h"

#if WITH_EDITOR
TArray<FRigVMLog::FLogEntry> FRigVMLog::GetEntries(EMessageSeverity::Type InSeverity, bool bIncludeHigherSeverity)
{
	return Entries.FilterByPredicate([InSeverity, bIncludeHigherSeverity](const FRigVMLog::FLogEntry& Entry)
	{
		if (bIncludeHigherSeverity)
		{
			return Entry.Severity <= InSeverity;
		}
		return Entry.Severity == InSeverity;
	});
}
#endif

void FRigVMLog::Reset()
{
#if WITH_EDITOR
	Entries.Reset();
	KnownMessages.Reset();
#endif
}

void FRigVMLog::Report(EMessageSeverity::Type InSeverity, const FName& InOperatorName, int32 InInstructionIndex, const FString& InMessage)
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
