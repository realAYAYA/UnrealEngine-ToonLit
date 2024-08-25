// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

struct RIGVM_API FRigVMLogSettings
{
	FRigVMLogSettings(EMessageSeverity::Type InSeverity, bool InLogOnce = true)
		: Severity(InSeverity)
		, bLogOnce(InLogOnce)
	{}

	EMessageSeverity::Type Severity;
	bool bLogOnce;
};

struct RIGVM_API FRigVMLog
{
public:

	FRigVMLog() {}
	virtual ~FRigVMLog() {}

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

	TArray<FLogEntry> GetEntries(EMessageSeverity::Type InSeverity = EMessageSeverity::Info, bool bIncludeHigherSeverity = true);
#endif

	virtual void Reset();
	virtual void Report(const FRigVMLogSettings& InLogSettings, const FName& InFunctionName, int32 InInstructionIndex, const FString& InMessage);

#if WITH_EDITOR
	void RemoveRedundantEntries();
#endif
};
