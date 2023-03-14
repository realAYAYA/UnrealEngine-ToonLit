// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

class FDataValidationContext : public FNoncopyable
{
public:
	struct FIssue
	{
		FText Message;
		EMessageSeverity::Type Severity;

		FIssue(const FText& InMessage, EMessageSeverity::Type InSeverity)
			: Message(InMessage)
			, Severity(InSeverity)
		{}
	};

	void AddWarning(const FText& Text) { Issues.Emplace(Text, EMessageSeverity::Warning); NumWarnings++; }
	void AddError(const FText& Text) { Issues.Emplace(Text, EMessageSeverity::Error); NumErrors++; }

	const TArray<FIssue>& GetIssues() const { return Issues; }
	uint32 GetNumWarnings() const { return NumWarnings; }
	uint32 GetNumErrors() const { return NumErrors; }

	void SplitIssues(TArray<FText>& Warnings, TArray<FText>& Errors) const
	{
		for (const FIssue& Issue : GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error)
			{
				Errors.Add(Issue.Message);
			}
			else if (Issue.Severity == EMessageSeverity::Warning)
			{
				Warnings.Add(Issue.Message);
			}
		}
	}

private:
	TArray<FIssue> Issues;
	uint32 NumWarnings = 0;
	uint32 NumErrors = 0;
};