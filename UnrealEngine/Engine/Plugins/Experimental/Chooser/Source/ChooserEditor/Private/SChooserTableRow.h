// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/Views/STableRow.h"

class UChooserTable;

namespace UE::ChooserEditor
{

class FChooserTableEditor;

struct FChooserTableRow
{
	FChooserTableRow(int32 i) { RowIndex = i; }
	int32 RowIndex;
};

class SChooserTableRow : public SMultiColumnTableRow<TSharedPtr<FChooserTableRow>>
{
public:
SLATE_BEGIN_ARGS(SChooserTableRow) {}
/** The list item for this row */
	SLATE_ARGUMENT(TSharedPtr<FChooserTableRow>, Entry)
	SLATE_ARGUMENT(UChooserTable*, Chooser)
	SLATE_ARGUMENT(FChooserTableEditor*, Editor)
	SLATE_END_ARGS()

	enum { SpecialIndex_AddRow = -1, SpecialIndex_Fallback = -2 };

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	TSharedPtr<FChooserTableRow> RowIndex;
	UChooserTable* Chooser;
	FChooserTableEditor* Editor;
	TSharedPtr<SBorder> CacheBorder;
	bool bDragActive = false;
	bool bDropSupported = false;
	bool bDropAbove = false;
};

}
