// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateColor;


/** A button widget used by DMX Control Console View to manage Fader Groups creation */
class SDMXControlConsoleEditorAddButton
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorAddButton)
	{}
		/** Called when this widget's button is clicked */
		SLATE_EVENT(FOnClicked, OnClicked)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	/** Manages this widget's button color, according to its hovering state */
	FSlateColor GetButtonColor() const;

	/** The delegate to excecute when this widget's button is clicked */
	FOnClicked OnClicked;
};
