// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class SAvaFontSelector;

class SAvaFontSearchSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaFontSearchSettingsMenu)
		: _AvaFontSelector()
		{
		}

		SLATE_ARGUMENT(TSharedPtr<SAvaFontSelector>, AvaFontSelector)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	TSharedRef<SWidget> CreateSettingsMenu() const;

	void BindCommands();

	void ShowMonospacedToggle_Execute();
	void ShowBoldToggle_Execute();
	void ShowItalicToggle_Execute();

	bool ShowMonospacedToggle_CanExecute() { return true; }
	bool ShowBoldToggle_CanExecute() { return true; }
	bool ShowItalicToggle_CanExecute() { return true; }

	bool ShowMonospacedToggle_IsChecked() const { return bIsShowMonospacedActive; }
	bool ShowBoldToggle_IsChecked() const { return bIsShowBoldActive; }
	bool ShowItalicToggle_IsChecked() const { return bIsShowItalicActive; }

	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<SAvaFontSelector> FontSelector;

	bool bIsShowMonospacedActive = false;

	bool bIsShowBoldActive = false;

	bool bIsShowItalicActive = false;
};
