// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXPixelMappingEditorCommon.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class STableViewBase;
class SInlineEditableTextBlock;
class SDMXPixelMappingHierarchyView;

class SDMXPixelMappingHierarchyItem
	: public STableRow<TSharedPtr<FDMXPixelMappingHierarchyItemWidgetModel>>
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingHierarchyItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, TSharedPtr<FDMXPixelMappingHierarchyItemWidgetModel> Model, TSharedPtr<SDMXPixelMappingHierarchyView> InHierarchyView);

private:
	FText GetItemText() const;

	/** Called when text is being committed to check for validity */
	bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage);

	/** Called when text is committed on the node */
	void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo);

	void OnRequestBeginRename();

	FReply OnDraggingWidget(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	FReply OnDropWidget(const FDragDropEvent& InDragDropEvent);

private:
	FDMXPixelMappingHierarchyWidgetModelWeakPtr Model;
	TSharedPtr<SDMXPixelMappingHierarchyView> HierarchyView;

	/** Edit box for the name. */
	TSharedPtr<SInlineEditableTextBlock> EditBox;
};
