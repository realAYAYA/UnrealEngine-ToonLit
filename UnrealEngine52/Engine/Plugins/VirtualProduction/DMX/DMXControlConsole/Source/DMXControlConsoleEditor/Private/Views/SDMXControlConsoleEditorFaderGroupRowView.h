// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleFaderGroupRow.h"

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SDMXControlConsoleEditorFaderGroupView;

class SHorizontalBox;


/** A widget which gathers a collection of Fader Groups */
class SDMXControlConsoleEditorFaderGroupRowView
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupRowView)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderGroupRow>& InFaderGroupRow);

	/** Gets the Fader Group Row this row is based on */
	UDMXControlConsoleFaderGroupRow* GetFaderGropuRow() const { return FaderGroupRow.Get(); }

	/** Gets the index of this row according to the referenced DMX Control Console */
	int32 GetRowIndex() const { return FaderGroupRow->GetRowIndex(); }

	/** Filters children by given search string  */
	void ApplyGlobalFilter(const FString& InSearchString);

protected:
	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Should be called when a Fader Group was added to the Fader Group Row this view displays */
	void OnFaderGroupAdded();

	/** Adds a Fader Group slot widget */
	void AddFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Should be called when a Fader Group was deleted from the Fader Group Row this view displays */
	void OnFaderGroupRemoved();

	/** Checks if FaderGroups array contains a reference to the given Fader Group */
	bool ContainsFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Weak Reference to this Fader Group Row */
	TWeakObjectPtr<UDMXControlConsoleFaderGroupRow> FaderGroupRow;

	/** Reference to the container widget of this Fader Group Row's Fader Group slots  */
	TSharedPtr<SHorizontalBox> FaderGroupsHorizontalBox;

	/** Array of weak references to Fader Group widgets */
	TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>> FaderGroupViews;
};
