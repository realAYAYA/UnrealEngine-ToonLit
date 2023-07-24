// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

#include "Widgets/SCompoundWidget.h"

class SDMXControlConsoleEditorFaderGroup;
class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFixturePatchMatrixCellFader;

struct FSlateColor;
class SHorizontalBox;


/** A widget which gathers a collection of Faders */
class SDMXControlConsoleEditorFaderGroupView
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupView)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderGroup>& InFaderGroup);

	/** Gets the Fader Group this Fader Group View is based on */
	UDMXControlConsoleFaderGroup* GetFaderGroup() const { return FaderGroup.Get(); }

	/** Gets the index of this Fader Group according to the referenced Fader Group Row */
	int32 GetIndex() const;

	/** Gets Fader Group's name */
	FString GetFaderGroupName() const;

	/** Filters children by given search string  */
	void ApplyGlobalFilter(const FString& InSearchString);

protected:
	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Generates Faders widget */
	TSharedRef<SWidget> GenerateFadersWidget();

	/** Shows all elements in this fader group view */
	void ShowAllElements();

	/** Should be called when an Element was added to the Fader Group this view displays */
	void OnElementAdded();

	/** Adds a Element slot widget */
	void AddElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element);

	/** Should be called when an Element was deleted from the Fader Group this view displays */
	void OnElementRemoved();

	/** Checks if Faders array contains a reference to the given Element */
	bool ContainsElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element);

	/** Notifies this Fader Group's owner row to add a new Fader Group */
	FReply OnAddFaderGroupClicked() const;

	/** Notifies this Fader Group's owner row to add a new Fader Group Row */
	FReply OnAddFaderGroupRowClicked() const;

	/** Notifies this Fader Group to add a new Fader */
	FReply OnAddFaderClicked();

	/** Gets fader group view border color */
	FSlateColor GetFaderGroupViewBorderColor() const;

	/** Gets Faders widget's visibility */
	EVisibility GetFadersWidgetVisibility() const;

	/** Gets add fader button visibility */
	EVisibility GetAddFaderButtonVisibility() const;

	/** Weak Reference to this Fader Group Row */
	TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup;

	/** Reference to the Fader Group main widget */
	TSharedPtr<SDMXControlConsoleEditorFaderGroup> FaderGroupWidget;

	/** Horizontal Box containing the Elements in this Fader Group */
	TSharedPtr<SHorizontalBox> ElementsHorizontalBox;

	/** Array of weak references to Element widgets */
	TArray<TWeakPtr<SWidget>> ElementWidgets;
};
