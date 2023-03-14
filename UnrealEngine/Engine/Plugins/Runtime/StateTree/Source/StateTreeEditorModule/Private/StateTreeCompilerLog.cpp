// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeCompilerLog.h"
#include "IMessageLogListing.h"
#include "Logging/LogCategory.h"
#include "Misc/UObjectToken.h"
#include "StateTreeState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeCompilerLog)

#define LOCTEXT_NAMESPACE "StateTreeEditor"

void FStateTreeCompilerLog::AppendToLog(IMessageLogListing* LogListing) const
{
	for (const FStateTreeCompilerLogMessage& StateTreeMessage : Messages)
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create((EMessageSeverity::Type)StateTreeMessage.Severity);

		if (StateTreeMessage.State != nullptr)
		{
			Message->AddToken(FUObjectToken::Create(StateTreeMessage.State, FText::FromName(StateTreeMessage.State->Name)));
		}

		if (StateTreeMessage.Item.ID.IsValid())
		{
			Message->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LogMessageItem", " {0}"), FText::FromName(StateTreeMessage.Item.Name))));
		}

		if (!StateTreeMessage.Message.IsEmpty())
		{
			Message->AddToken(FTextToken::Create(FText::FromString(StateTreeMessage.Message)));
		}

		LogListing->AddMessage(Message);
	}
}

void FStateTreeCompilerLog::DumpToLog(const FLogCategoryBase& Category) const
{
	for (const FStateTreeCompilerLogMessage& StateTreeMessage : Messages)
	{
		FString Message;
		
		if (StateTreeMessage.State != nullptr)
		{
			Message += FString::Printf(TEXT("State '%s': "), *StateTreeMessage.State->Name.ToString());
		}

		if (StateTreeMessage.Item.ID.IsValid())
		{
			Message += FString::Printf(TEXT("%s '%s': "), *UEnum::GetDisplayValueAsText(StateTreeMessage.Item.DataSource).ToString(), *StateTreeMessage.Item.Name.ToString());
		}

		Message += StateTreeMessage.Message;

		switch (StateTreeMessage.Severity)
		{
		case EMessageSeverity::Error:
			UE_LOG_REF(Category, Error, TEXT("%s"), *Message);
			break;
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
			UE_LOG_REF(Category, Warning, TEXT("%s"), *Message);
			break;
		case EMessageSeverity::Info:
		default:
			UE_LOG_REF(Category, Log, TEXT("%s"), *Message);
			break;
		};
	}
}


#undef LOCTEXT_NAMESPACE

