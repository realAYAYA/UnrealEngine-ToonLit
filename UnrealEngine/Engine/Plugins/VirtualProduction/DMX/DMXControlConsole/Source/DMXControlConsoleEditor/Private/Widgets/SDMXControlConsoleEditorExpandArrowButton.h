// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FReply;
struct FSlateBrush;


/** Delegate invoked when a user needs to know current ExpandArrowButton expansion state. */
DECLARE_DELEGATE_OneParam(FDMXControleConsolEditorExpandArrowButtonDelegate, bool)

namespace UE::DMX::Private
{
	/** A button widget used by DMX Control Console Views to expand/collapse widgets */
	class SDMXControlConsoleEditorExpandArrowButton
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorExpandArrowButton)
			: _IsExpanded(true)
			{}
			/** True if the button is in expand state */
			SLATE_ARGUMENT(bool, IsExpanded)

			/** Called when this widget's button is clicked */
			SLATE_EVENT(FDMXControleConsolEditorExpandArrowButtonDelegate, OnExpandClicked)

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

		/** True if the button is in expand state */
		bool bIsExpanded = true;

		/** The delegate to excecute when this widget's button is clicked */
		FDMXControleConsolEditorExpandArrowButtonDelegate OnExpandClicked;
	};
}
