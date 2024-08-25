// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "AvaFontSelectorCommands"

class FAvaFontSelectorCommands
	: public TCommands<FAvaFontSelectorCommands>
{
public:
	FAvaFontSelectorCommands()
		: TCommands<FAvaFontSelectorCommands>(TEXT("AvaFontSelector")
		, LOCTEXT("MotionDesignFontSelector", "Motion Design Font Selector")
		, NAME_None
		, FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> ShowMonospacedFonts;
	TSharedPtr<FUICommandInfo> ShowBoldFonts;
	TSharedPtr<FUICommandInfo> ShowItalicFonts;
};

#undef LOCTEXT_NAMESPACE
