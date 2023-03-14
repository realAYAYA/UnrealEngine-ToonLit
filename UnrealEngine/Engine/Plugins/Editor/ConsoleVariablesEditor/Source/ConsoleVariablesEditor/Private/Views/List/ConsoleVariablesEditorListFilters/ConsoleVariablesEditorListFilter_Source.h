// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IConsoleVariablesEditorListFilter.h"

#define LOCTEXT_NAMESPACE "ConsoleVariablesEditor"

class FConsoleVariablesEditorListFilter_Source : public IConsoleVariablesEditorListFilter
{
public:

	FConsoleVariablesEditorListFilter_Source(const EConsoleVariableFlags InSourceFlag)
	: SourceFlag(InSourceFlag){}

	virtual FString GetFilterName() override
	{
		return FConsoleVariablesEditorCommandInfo::ConvertConsoleVariableSetByFlagToText(SourceFlag).ToString();
	}

	virtual FText GetFilterButtonLabel() override
	{
		return FText::Format(LOCTEXT("ShowSourceTextFilterFormat", "Show {0}"), FText::FromString(GetFilterName()));
	}

	virtual FText GetFilterButtonToolTip() override
	{
		return FText::Format(
			LOCTEXT("ShowSourceTextFilterTooltipFormat", "Show rows that have a Source field matching '{0}'"),
			FText::FromString(GetFilterName()));
	}

	virtual bool DoesItemPassFilter(const FConsoleVariablesEditorListRowPtr& InItem) override
	{
		if (InItem.IsValid())
		{
			if (const TSharedPtr<FConsoleVariablesEditorCommandInfo> PinnedCommand = InItem->GetCommandInfo().Pin())
			{
				const EConsoleVariableFlags Source = PinnedCommand->GetSource();
				return Source == SourceFlag;
			}
		}

		return false;
	}
	
private:

	EConsoleVariableFlags SourceFlag;
};

#undef LOCTEXT_NAMESPACE
