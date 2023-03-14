// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ChaosLog.h"
#include "Logging/LogMacros.h"

namespace Chaos
{
class CHAOS_API FErrorReporter
{
public:
	FErrorReporter(FString ErrorPrefix = "")
	: bEncountedErrors(false)
	, bUnhandledErrors(false)
	, Prefix(ErrorPrefix)
	{
	}

	void HandleLatestError()
	{
		bUnhandledErrors = false;
	}

	bool ContainsUnhandledError() const
	{
		return bUnhandledErrors;
	}

	void ReportLog(const TCHAR* ErrorMsg)
	{
		if(Prefix != "")
		{
			UE_LOG(LogChaos, Log, TEXT("ErrorReporter (%s): %s"), *Prefix, ErrorMsg);
		}
		else
		{
			UE_LOG(LogChaos, Log, TEXT("ErrorReporter: %s"), *Prefix, ErrorMsg);
		}
	}

	void ReportWarning(const TCHAR* ErrorMsg)
	{
		if (Prefix != "")
		{
			UE_LOG(LogChaos, Warning, TEXT("ErrorReporter (%s): %s"), *Prefix, ErrorMsg);
		}
		else
		{
			UE_LOG(LogChaos, Warning, TEXT("ErrorReporter: %s"), *Prefix, ErrorMsg);
		}
	}

	void ReportError(const TCHAR* ErrorMsg)
	{
		ReportWarning(ErrorMsg);
		bEncountedErrors = true;
		bUnhandledErrors = true;
	}

	bool EncounteredAnyErrors() const
	{
		return bEncountedErrors;
	}

	void SetPrefix(FString NewPrefix)
	{
		Prefix = NewPrefix;
	}

	FString GetPrefix()
	{
		return Prefix;
	}

private:
	bool bEncountedErrors;
	bool bUnhandledErrors;
	FString Prefix;
};
}
