// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SWindow;

class WAVEFORMEDITORWIDGETS_API SWaveformEditorMessageDialog : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SWaveformEditorMessageDialog) {}

	/** A reference to the parent window */
	SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

	/** The Message To Display */
	SLATE_ARGUMENT(FText, MessageToDisplay)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private: 
	/** The parent window of this widget */
	TWeakPtr<SWindow> ParentWindowPtr;

	bool CanPressCloseButton() const;
	
	FReply OnCloseButtonPressed() const;
};