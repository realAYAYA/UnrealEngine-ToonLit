// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

#include "Widgets/SCompoundWidget.h"

enum class EDMXControlConsoleEditorViewMode : uint8;
struct FOptionalSize;
struct FSlateBrush;
struct FSlateColor;
class IDMXControlConsoleFaderGroupElement;
class SDMXControlConsoleEditorExpandArrowButton;
class SDMXControlConsoleEditorFaderGroupToolbar;
class SHorizontalBox;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFixturePatchMatrixCellFader;
class UDMXEntityFixturePatch;


/** A widget which gathers a collection of Faders */
class SDMXControlConsoleEditorFaderGroupView
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupView)
	{}

	SLATE_END_ARGS()

	/** Constructor */
	SDMXControlConsoleEditorFaderGroupView();

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderGroup>& InFaderGroup);

	/** Gets the Fader Group this Fader Group View is based on */
	UDMXControlConsoleFaderGroup* GetFaderGroup() const { return FaderGroup.Get(); }

	/** Gets the index of this Fader Group according to the referenced Fader Group Row */
	int32 GetIndex() const;

	/** Gets Fader Group's name */
	FString GetFaderGroupName() const;

	/** Gets current ViewMode */
	EDMXControlConsoleEditorViewMode GetViewMode() const { return ViewMode; }

	/** True if a new Fader Group can be added next to this */
	bool CanAddFaderGroup() const;

	/** True if a new Fader Group can be added on next row */
	bool CanAddFaderGroupRow() const;

	/** True if a new Fader can be added */
	bool CanAddFader() const;

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Generates ElementsHorizontalBox widget */
	TSharedRef<SWidget> GenerateElementsWidget();

	/** Gets wheter this Fader Group is selected or not */
	bool IsSelected() const;

	/** Gets a reference to the toolbar's ExpandArrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> GetExpandArrowButton() const;

	/** Should be called when an Element was added to the Fader Group this view displays */
	void OnElementAdded();

	/** Adds a Element slot widget */
	void AddElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element);

	/** Should be called when an Element was deleted from the Fader Group this view displays */
	void OnElementRemoved();

	/** Checks if Faders array contains a reference to the given Element */
	bool ContainsElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element);

	/** Updates this widget to the last saved expansion state from the model */
	void UpdateExpansionState();

	/** Called when the Expand Arrow button is clicked */
	void OnExpandArrowClicked(bool bExpand);

	/** Adds a new Fader Group to the owner row */
	void OnAddFaderGroup() const;

	/** Adds a new Fader Group Row next to the owner row */
	void OnAddFaderGroupRow() const;

	/** Called when Fader Group Fixture Patch has changed */
	void OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* InFaderGroup, UDMXEntityFixturePatch* FixturePatch);

	/** Notifies this Fader Group's owner row to add a new Fader Group */
	FReply OnAddFaderGroupClicked() const;

	/** Notifies this Fader Group's owner row to add a new Fader Group Row */
	FReply OnAddFaderGroupRowClicked() const;

	/** Notifies this Fader Group to add a new Fader */
	FReply OnAddFaderClicked();

	/** Called when Fader Groups view mode is changed */
	void OnViewModeChanged();

	/** True if the given View Mode matches the current one */
	bool IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const;

	/** Gets the height of the FaderGroup view according to the current Faders View Mode  */
	FOptionalSize GetFaderGroupViewHeightByFadersViewMode() const;

	/** Gets fader group view border color */
	FSlateColor GetFaderGroupViewBorderColor() const;

	/** Changes brush when this widget is hovered */
	const FSlateBrush* GetFaderGroupViewBorderImage() const;

	/** Changes background brush when this widget is hovered */
	const FSlateBrush* GetFaderGroupViewBackgroundBorderImage() const;

	/** Gets visibility according to the given View Mode */
	EVisibility GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const;

	/** Gets visibility for each Element widget in this view */
	EVisibility GetElementWidgetVisibility(const TScriptInterface<IDMXControlConsoleFaderGroupElement> Element) const;

	/** Manages horizontal Add Button widget's visibility */
	EVisibility GetAddButtonVisibility() const;

	/** Manages vertical Add Button widget's visibility */
	EVisibility GetAddRowButtonVisibility() const;

	/** Gets ElementsHorizontalBox widget visibility */
	EVisibility GetElementsHorizontalBoxVisibility() const;

	/** Gets add fader button visibility */
	EVisibility GetAddFaderButtonVisibility() const;

	/** Current view mode */
	EDMXControlConsoleEditorViewMode ViewMode;

	/** Weak Reference to this Fader Group Row */
	TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup;

	/** Horizontal Box containing the Elements in this Fader Group */
	TSharedPtr<SDMXControlConsoleEditorFaderGroupToolbar> FaderGroupToolbar;

	/** Horizontal Box containing the Elements in this Fader Group */
	TSharedPtr<SHorizontalBox> ElementsHorizontalBox;

	/** Array of weak references to Element widgets */
	TArray<TWeakPtr<SWidget>> ElementWidgets;
};
