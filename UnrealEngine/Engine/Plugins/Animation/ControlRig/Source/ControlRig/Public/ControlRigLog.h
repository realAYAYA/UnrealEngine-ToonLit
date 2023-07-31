// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

struct CONTROLRIG_API FControlRigLog
{
public:

	FControlRigLog() {}
	virtual ~FControlRigLog() {}

#if WITH_EDITOR
	struct FLogEntry
	{
		FLogEntry(EMessageSeverity::Type InSeverity, const FName& InFunctionName, int32 InInstructionIndex, const FString& InMessage)
		: Severity(InSeverity)
		, FunctionName(InFunctionName)
		, InstructionIndex(InInstructionIndex)
		, Message(InMessage)
		{}
		
		EMessageSeverity::Type Severity;
		FName FunctionName;
		int32 InstructionIndex;
		FString Message;
	};
	TArray<FLogEntry> Entries;
	TMap<FString, bool> KnownMessages;
#endif

	virtual void Reset();
	virtual void Report(EMessageSeverity::Type InSeverity, const FName& InFunctionName, int32 InInstructionIndex, const FString& InMessage);
};
