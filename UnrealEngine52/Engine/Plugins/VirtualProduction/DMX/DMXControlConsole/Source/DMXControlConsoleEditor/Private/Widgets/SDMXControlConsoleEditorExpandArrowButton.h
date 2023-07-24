// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FReply;
struct FSlateBrush;


/** A button widget used by DMX Control Console Views to expand/collapse widgets */
class SDMXControlConsoleEditorExpandArrowButton
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(ExpandArrowButtonDelegate, bool)

	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorExpandArrowButton)
	{}
		/** Called when this widget's button is clicked */
		SLATE_EVENT(ExpandArrowButtonDelegate, OnExpandClicked)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Sets expand state of the button */
	void SetExpandArrow(bool bExpand);

	/** Toggles the expand state of the button */
	void ToggleExpandArrow();

	/** Gets the button state */
	bool IsExpanded() const { return bIsExpanded; }

private:
	/** Called when this widget's button is clicked */
	FReply OnExpandArrowClicked();

	/** Gets brush for the Expander Arrow Button */
	const FSlateBrush* GetExpandArrowImage() const;

	/** The delegate to excecute when this widget's button is clicked */
	ExpandArrowButtonDelegate OnExpandClicked;

	/** True if the button is in expand state */
	bool bIsExpanded = true;
};
