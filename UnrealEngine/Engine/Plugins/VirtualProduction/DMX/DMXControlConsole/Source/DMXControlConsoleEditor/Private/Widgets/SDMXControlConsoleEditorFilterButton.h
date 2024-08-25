// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FReply;
struct FSlateColor;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFilterModel;

	/** A button widget to display a filter in a DMX Control Console */
	class SDMXControlConsoleEditorFilterButton
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFilterButton)
			{}

			/** Called when the disable all filters action is performed */
			SLATE_EVENT(FSimpleDelegate, OnDisableAllFilters)

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleFilterModel>& InFilterModel);

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		//~ End of SWidget interface

	private:
		/** Generates a context menu widget for filter options */
		TSharedRef<SWidget> GenerateFilterButtonMenuWidget();

		/** True if the filter displayed by this button is a user filter */
		bool IsUserFilter() const;

		/** True if the filter displayed by this button is enabled */
		bool IsFilterEnabled() const;

		/** Sets the enable state of the filter displayed by this button */
		void SetIsFilterEnabled(bool bEnable);

		/** Gets the label name of the filter displayed by this button as a text */
		FText GetFilterLabelAsText() const;

		/** Gets the string associated to the filter displayed by this button as a text */
		FText GetFilterStringAsText() const;

		/** Called when this widget's button is clicked */
		FReply OnFilterButtonClicked();

		/** Called when the remove filter option is selected */
		void OnRemoveFilter() const;

		/** Called when the disable all filters but this option is selected */
		void OnDisableAllFiltersButThis();

		/** Called when the disable all filters option is selected */
		void OnDisableAllFilters() const;

		/** Gets the editor color of the filter displayed by this button depending on its current state */
		FSlateColor GetFilterButtonColor() const;

		/** Reference to the filter model this widget is based on */
		TWeakPtr<FDMXControlConsoleFilterModel> WeakFilterModel;

		/** The delegate to excecute when the disable all filters action is performed */
		FSimpleDelegate OnDisableAllFiltersDelegate;
	};
}
