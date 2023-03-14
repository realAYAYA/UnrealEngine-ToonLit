// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CompilationMessageCache.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AssertionMacros.h"


void FCompilationMessageCache::AddMessage(const FText& InMessage, const TArray<const UCustomizableObjectNode*>& InArrayNode, EMessageSeverity::Type MessageSeverity /* = EMessageSeverity::Warning */)
{
	// Skip message if an identical one has already been reported
	const FLoggedMessage Cached = { InMessage, InArrayNode, MessageSeverity };
	if (LoggedMessages.Contains(Cached))
	{
		return;
	}

	LoggedMessages.Add(Cached);

	switch (MessageSeverity)
	{
	case EMessageSeverity::Error:
		ErrorCount++;
		break;
	case EMessageSeverity::PerformanceWarning:
		PerformanceWarningCount++;
		break;
	case EMessageSeverity::Warning:
		WarningCount++;
		break;
	case EMessageSeverity::Info:
		// not handled at the moment
		break;
	default:
		checkNoEntry();
		break;
	}
}


void FCompilationMessageCache::GetMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const
{
	if (LoggedMessages.IsEmpty())
	{
		return;
	}

	for (const FLoggedMessage& Log : LoggedMessages)
	{
		switch (Log.Severity)
		{
		case EMessageSeverity::Info:
			// not handled at the moment -> not required
			break;
		
		case EMessageSeverity::Error:
			OutErrorMessages.Add(Log.Message);
			break;

			// Both performance and standard warnings get treated as warnings.
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
			OutWarningMessages.Add(Log.Message);
			break;

		default:
			// ERROR : severity type not defined
			checkNoEntry();
		}
	}	
}


void FCompilationMessageCache::ClearMessagesArray()
{
	LoggedMessages.Empty();
}


void FCompilationMessageCache::ClearMessageCounters()
{
	PerformanceWarningCount = 0;
	WarningCount = 0;
	ErrorCount = 0;
}

uint32 FCompilationMessageCache::GetWarningCount(bool bIncludePerformanceWarnings) const
{
	if (bIncludePerformanceWarnings)
	{
		return WarningCount + PerformanceWarningCount;
	}
	
	return WarningCount;
}

uint32 FCompilationMessageCache::GetErrorCount() const
{
	return ErrorCount;
}