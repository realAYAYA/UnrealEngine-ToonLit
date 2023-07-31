// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Logging/TokenizedMessage.h"

// Forward declarations
class UCustomizableObjectNode;

/** Class designed to allow easy access to the logs produced during the compilation of a Customizable Object */
class CUSTOMIZABLEOBJECTEDITOR_API FCompilationMessageCache
{
public:

	/** Method designed to serve as a way to add a new message to the collection of messages the compiler has produced as
	 * part of the compilation process.
	 * @param InMessage The message's text.
	 * @param InArrayNode The node that is related with the InMessage provided.
	 * @param MessageSeverity Severity of the message.
	 */
	 void AddMessage(const FText& InMessage, const TArray<const UCustomizableObjectNode*>& InArrayNode, EMessageSeverity::Type MessageSeverity = EMessageSeverity::Warning);
	
	
	/** Provides the caller with separated arrays with all the warning and error messages.
	 * @param OutWarningMessages Array with messages that can be interpreted as Warnings.
	 * @param OutErrorMessages Array with messages that can be interpreted as errors.
	 * @note Performance warnings are considered warnings so they get returned
	 */
	void GetMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const;


// protected:

	/** Clear the array of logs */
	void ClearMessagesArray();

	/** Clear the counters that show how many errors and warnings the array contains */
	void ClearMessageCounters();

	/** Provides with the amount of messages cached as warnings
	* @param bIncludePerformanceWarnings If true the count will include performance and non performance related warnings.
	* If false only standard warnings will be returned to the caller.
	* @return Amount of warning messages cached.
	*/
	uint32 GetWarningCount(bool bIncludePerformanceWarnings) const;

	/** Provides the caller with the amount of messages cached as errors
	 * @return Amount of error messages cached.
	 */
	uint32 GetErrorCount() const;
	
private:
	
	// List of all logged messages to ensure uniqueness
	struct FLoggedMessage
	{
		FText Message;
		TArray<const UCustomizableObjectNode*> Context;
		EMessageSeverity::Type Severity;
	
		bool operator==( const FLoggedMessage& o) const
		{
			return Message.EqualTo(o.Message) && Context == o.Context && Severity == o.Severity;
		}
	};

	/** Array with all the logged messages stored. */
	TArray<FLoggedMessage> LoggedMessages;

	// Counters for the type of messages we have on LoggedMessages
	uint32 PerformanceWarningCount = 0;
	uint32 WarningCount = 0;
	uint32 ErrorCount = 0;
	
};