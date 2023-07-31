// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FDMXEditor;
class FDMXFixturePatchNode;
class UDMXEntityFixturePatch;
class UDMXLibrary;

struct FSlateColorBrush;
class SBorder;
template <typename OptionType> class SComboBox;


class SDMXFixturePatchFragment
	: public SCompoundWidget 
{
public:
	SLATE_BEGIN_ARGS(SDMXFixturePatchFragment)
		: _DMXEditor(nullptr)
		, _IsHead(true)
		, _IsTail(true)
		, _IsText(false)
		, _IsConflicting(false)
		, _Column(-1)
		, _Row(-1)
		, _ColumnSpan(1)
		, _bHighlight(false)
	{}
		SLATE_ARGUMENT(TWeakPtr<FDMXEditor>, DMXEditor)

		SLATE_ARGUMENT(bool, IsHead)

		SLATE_ARGUMENT(bool, IsTail)

		SLATE_ARGUMENT(bool, IsText)

		SLATE_ARGUMENT(bool, IsConflicting)

		SLATE_ARGUMENT(int32, Column)

		SLATE_ARGUMENT(int32, Row)

		SLATE_ARGUMENT(int32, ColumnSpan)

		SLATE_ARGUMENT(bool, bHighlight)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXFixturePatchNode>& InFixturePatchNode, const TArray<TSharedPtr<FDMXFixturePatchNode>>& InFixturePatchNodeGroup);

	/** Refreshes the widget */
	void Refresh(const TArray<TSharedPtr<FDMXFixturePatchNode>>& InFixturePatchNodeGroup);

	/** Sets wether the fragment should be highlit */
	void SetHighlight(bool bEnabled) { bHighlight = bEnabled; }

	/** Sets if the fragment is conflicting with others */
	void SetConflicting(bool bConflicting) { bIsConflicting = bConflicting; if (Column == 0 && Row == 0 && bIsConflicting) UE_LOG(LogTemp, Warning, TEXT("%s set to conflicting"), *FixturePatchNameText.ToString()); }

	/** Returns the Column of the fragment */
	int32 GetColumn() const { return Column; }

	/** Sets the Column of the fragment */
	void SetColumn(int32 NewColumn) { Column = NewColumn; }

	/** Returns the Row of the fragment */
	int32 GetRow() const { return Row; }

	/** Sets the Row of the fragment */
	void SetRow(int32 NewRow) { Row = NewRow; }

	/** Returns the Column span of the fragment */
	int32 GetColumnSpan() const { return ColumnSpan; }

	/** Sets the Column span of the fragment */
	void SetColumnSpan(int32 NewColumnSpan) { ColumnSpan = NewColumnSpan; }

private:
	/** Called when Fixture Patch Shared Data selected a Fixture Patch */
	void OnFixturePatchSharedDataSelectedFixturePatch();

	/** Updates the Fixture Patch Name Text */
	void UpdateFixturePatchNameText();

	/** Updates the Node Group Text */
	void UpdateNodeGroupText();

	/** Updates the Padding of the outermost Border */
	void UpdateBorderPadding();

	/** Updates the bTopmost memeber from the ZOrder of the node */
	void UpdateIsTopmost();

	/** Returns the Border image */
	const FSlateBrush& GetBorderImage() const;

	/** Gets the Text Widget */
	TSharedRef<SWidget> CreateTextWidget();

	/** If true, the patch has the highest ZOrder in its node group */
	bool bIsTopmost = false;

	/** If true this is the Head of the Fixture Patch */
	bool bIsHead = false;

	/** If true this is the Tail of the Fixture Patch */
	bool bIsTail = false;

	/** If true, this fragment shows text */
	bool bIsText;

	/** Column of the Fragment */
	int32 Column;

	/** Row of the Fragment */
	int32 Row;

	/** Column span of the Fragment */
	int32 ColumnSpan;

	/** If true, the Widget shows a highlight */
	bool bHighlight = false;

	/** If true, the Widget displays a conflict background */
	bool bIsConflicting = false;

	/** The Fixture Patch Name Text the Node displays */
	FText FixturePatchNameText;

	/** The Node Group Text the Node displays */
	FText NodeGroupText;

	/** The padding of the outermost border */
	FMargin BorderPadding;
	
	/** The border that holds the color or text content */
	TSharedPtr<SBorder> ContentBorder;

	/** Color Brush the widget uses */
	TSharedPtr<FSlateColorBrush> ColorBrush;

	/** Fixture Patch Node being displayed by this widget */
	TSharedPtr<FDMXFixturePatchNode> FixturePatchNode;

	/** The group of nodes (same Universe, same Channel) this widget belongs to */
	TArray<TSharedPtr<FDMXFixturePatchNode>> FixturePatchNodeGroup;

	/** Weak DMXEditor refrence */
	TWeakPtr<FDMXEditor> DMXEditorPtr;
};
