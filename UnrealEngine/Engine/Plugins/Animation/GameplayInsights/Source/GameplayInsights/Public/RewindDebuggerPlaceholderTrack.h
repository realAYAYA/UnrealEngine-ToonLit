// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RewindDebuggerTrack.h"

class GAMEPLAYINSIGHTS_API FRewindDebuggerPlaceholderTrack : public RewindDebugger::FRewindDebuggerTrack
{
public:
	FRewindDebuggerPlaceholderTrack(const FName& InObjectName, const FText& InDisplayName)
		: ObjectName(InObjectName), DisplayName(InDisplayName)
	{
	}

private:
	virtual FName GetNameInternal() const override { return ObjectName; }
	virtual FText GetDisplayNameInternal() const override { return DisplayName; }
	
	FName ObjectName;
	FText DisplayName;
};
