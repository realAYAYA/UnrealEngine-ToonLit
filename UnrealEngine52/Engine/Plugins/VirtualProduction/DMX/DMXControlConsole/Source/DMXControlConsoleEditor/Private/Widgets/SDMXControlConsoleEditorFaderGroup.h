// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Widgets/Input/SButton.h"

class SDMXControlConsoleEditorFaderGroupView;
class SDMXControlConsoleEditorExpandArrowButton;
class UDMXControlConsoleFaderGroup;

struct FSlateBrush;
struct FSlateColor;
class SEditableTextBox;


/** Base Fader Group UI widget */
class SDMXControlConsoleEditorFaderGroup
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroup)
	{}
		/** Executed when a Fader Group widget is added */
		SLATE_EVENT(FOnClicked, OnAddFaderGroup)

		/** Executed when a new Fader Group Row widget is added */
		SLATE_EVENT(FOnClicked, OnAddFaderGroupRow)
	
		/** Executed when this Fader Group widget is expanded */
		SLATE_EVENT(FOnClicked, OnExpanded)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView);

	/** Gets a reference to this widget's ExpandArrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton>& GetExpandArrowButton() { return ExpandArrowButton; }

protected:
	//~ Begin SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	//~ End of SWidget interface

private:
	/** Gets reference to the Fader Group */
	UDMXControlConsoleFaderGroup* GetFaderGroup() const;

	/** Gets wheter this Fader Group is selected or not */
	bool IsSelected() const;

	/** Called when fader group selection changes */
	void OnSelectionChanged();

	/** Gets current FaderGroupName */
	FText OnGetFaderGroupNameText() const;

	/** Called when the fader name changes */
	void OnFaderGroupNameCommitted(const FText& NewName, ETextCommit::Type InCommit);

	/** Manages vertical Add Button widget's visibility */
	EVisibility GetAddRowButtonVisibility() const;

	/** Gets border color according to the Fader Group */
	FSlateColor GetFaderGroupBorderColor() const;

	/** Changes brush when this widget is hovered */
	const FSlateBrush* GetFaderGroupBorderImage() const;

	/** Weak Reference to this Fader Group Row */
	TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView;

	/** Faders Widget's expander arrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

	/** Shows/Modifies Fader Group Name */
	TSharedPtr<SEditableTextBox> FaderGroupNameTextBox;

	// Slate Arguments
	FOnClicked OnAddFaderGroup;
	FOnClicked OnAddFaderGroupRow;
};
