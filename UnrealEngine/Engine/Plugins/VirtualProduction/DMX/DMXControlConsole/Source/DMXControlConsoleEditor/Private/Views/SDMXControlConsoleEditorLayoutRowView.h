// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Widgets/SCompoundWidget.h"

class SDMXControlConsoleEditorFaderGroupView;
class SHorizontalBox;


/** A widget which gathers a collection of Fader Groups */
class SDMXControlConsoleEditorLayoutRowView
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayoutRowView)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow>& InLayoutRow);

	/** Gets the Layout Row this row is based on */
	UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow() const { return LayoutRow.Get(); }

	/** Finds FaderGroupView by the given FaderGroup, if valid */
	TSharedPtr<SDMXControlConsoleEditorFaderGroupView> FindFaderGroupView(const UDMXControlConsoleFaderGroup* FaderGroup) const;

protected:
	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Refreshes layout row */
	void Refresh();

	/** Should be called when a Fader Group was added to the Layout Row this view displays */
	void OnFaderGroupAdded();

	/** Adds a Fader Group slot widget */
	void AddFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Should be called when a Fader Group was deleted from the Layout Row this view displays */
	void OnFaderGroupRemoved();

	/** Checks if FaderGroups array contains a reference to the given Fader Group */
	bool ContainsFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Gets visibility for each FaderGroupView widget in this row */
	EVisibility GetFaderGroupViewVisibility(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Weak Reference to this Layout Row */
	TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow;

	/** Reference to the container widget of this Layout Row's Fader Group slots  */
	TSharedPtr<SHorizontalBox> FaderGroupsHorizontalBox;

	/** Array of weak references to Fader Group widgets */
	TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>> FaderGroupViews;
};
