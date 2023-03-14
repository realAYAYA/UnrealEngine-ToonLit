// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/STreeView.h"
#include "SGeometryCollectionTreeItem.h"


class SGeometryCollectionTreeView : public STreeView<FGeometryCollectionTreeItem>
{

public:
	/**
	* Construct this widget.  Called by the SNew() Slate macro.
	*
	* @param	InArgs		Declaration used by the SNew() macro to construct this widget
	*/
	void Construct(const FArguments& InArgs);

private:
	/** Weak reference to the outliner widget that owns this list */
	// TWeakPtr<SGeometryCollection> OutlinerWeak;
};

/** Widget that represents a row in the outliner's tree control.  Generates widgets for each column on demand. */
class SGeometryCollectionTreeRow : public SMultiColumnTableRow<FGeometryCollectionTreeItem>
{
public:

	SLATE_BEGIN_ARGS(SGeometryCollectionTreeRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(FGeometryCollectionTreeItem, Item)

	SLATE_END_ARGS()

	/**
	* Construct this widget.  Called by the SNew() Slate macro.
	*
	* @param	InArgs		Declaration used by the SNew() macro to construct this widget
	* @param	OutlinerTreeView		The CurveEditor Outliner which owns this widget.
	
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<SGeometryCollectionTreeView>& OutlinerTreeView);

	// SMultiColumnTableRow Interface
	/** Generates a widget for the specified column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	// ~SMultiColumnTableRow Interface

private:
	FText GetCurveName() const;

private:
	/** Weak reference to the outliner widget that owns our list */
	// TWeakPtr<SGeometryCollection> GeometryCollectionWeak;

	/** The item associated with this row of data. */
	TWeakPtr<FGeometryCollectionTreeItem> Item;
};
