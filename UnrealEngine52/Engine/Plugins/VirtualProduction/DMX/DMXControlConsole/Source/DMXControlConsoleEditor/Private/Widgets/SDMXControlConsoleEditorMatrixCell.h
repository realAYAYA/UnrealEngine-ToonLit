// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SDMXControlConsoleEditorExpandArrowButton;
class SDMXControlConsoleEditorFader;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFixturePatchCellAttributeFader;
class UDMXControlConsoleFixturePatchMatrixCell;

struct FSlateColor;
class SHorizontalBox;
class SInlineEditableTextBlock;


/** Individual Matrix Cell UI class */
class SDMXControlConsoleEditorMatrixCell
	: public SCompoundWidget
{	
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorMatrixCell)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFixturePatchMatrixCell>& InMatrixCell);

	/** Gets reference to the Matrix Cell showned by this widget */
	UDMXControlConsoleFixturePatchMatrixCell* GetMatrixCell() { return MatrixCell.Get(); }

	/** Gets a reference to this widget's ExpandArrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton>& GetExpandArrowButton() { return ExpandArrowButton; }

	/** Filters children by given search string  */
	void ApplyGlobalFilter(const FString& InSearchString);

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Should be called when a Cell Attribute Fader was added to the Matrix Cell Fader this widget displays */
	void OnCellAttributeFaderAdded();

	/** Adds a Cell Attribute Fader slot widget */
	void AddCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader);

	/** Should be called when a Cell Attribute Fader was deleted from the Matrix Cell Fader this widget displays */
	void OnCellAttributeFaderRemoved();

	/** Checks if CellAttributeFaders array contains a reference to the given Cell Attribute Fader */
	bool ContainsCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader);

	/** Gets wheter this Matrix Cell Fader is selected or not */
	bool IsSelected() const;

	/** Returns true if any of this Matrix Cell's Cell Attribute Fader is selected */
	bool IsAnyCellAttributeFaderSelected() const;

	/** Gets Matrix Cell ID as text */
	FText GetMatrixCellLabelText() const;

	/** Gets label background color */
	FSlateColor GetLabelBorderColor() const;

	/** Gets visibility of CellAttributeFadersHorizontalBox widget */
	EVisibility GetCellAttributeFadersHorizontalBoxVisibility() const;

	/** Gets widget border brush */
	const FSlateBrush* GetBorderImage() const;

	/** Reference to the Matrix Cell being displayed */
	TWeakObjectPtr<UDMXControlConsoleFixturePatchMatrixCell> MatrixCell;

	/** Reference to the Cell Attribute Faders main widget */
	TSharedPtr<SHorizontalBox> CellAttributeFadersHorizontalBox;

	/** Array of Cell Attribute Fader widgets */
	TArray<TWeakPtr<SDMXControlConsoleEditorFader>> CellAttributeFaderWidgets;

	/** Reference to ExpandArrow button used to show/hide Matrix Cell */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;
};
