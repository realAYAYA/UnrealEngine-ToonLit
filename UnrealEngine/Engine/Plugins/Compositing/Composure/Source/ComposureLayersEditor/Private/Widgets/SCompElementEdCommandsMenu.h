// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h" // for SLATE_BEGIN_ARGS(), SLATE_ARGUMENT()

class FCompElementCollectionViewModel;

class SCompElementEdCommandsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCompElementEdCommandsMenu)
		: _CloseWindowAfterMenuSelection(true) {}

		SLATE_ARGUMENT(bool, CloseWindowAfterMenuSelection)
	SLATE_END_ARGS()

	/** Construct this widget. Called by the SNew() Slate macro. */
	void Construct(const FArguments& InArgs, const TSharedRef<FCompElementCollectionViewModel> InViewModel);

private:
	/** The UI logic of the panel that is not Slate specific */
	TSharedPtr<FCompElementCollectionViewModel> ViewModel;
};

