// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Views/SPropertyAnimatorCoreEditorPropertiesView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Views/STreeView.h"

class SPropertyAnimatorCoreEditorPropertiesView;
class UToolMenu;

class SPropertyAnimatorCoreEditorPropertiesViewTableRow : public SMultiColumnTableRow<FPropertiesViewItemPtr>
{
	friend class SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow;

public:
	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorPropertiesViewTableRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView,
		TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> InView, FPropertiesViewItemPtr InItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

protected:
	TOptional<EItemDropZone> OnPropertyCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FPropertiesViewItemPtr InItem);

	int32 OnPropertyPaintDropIndicator(EItemDropZone InDropZone, const FPaintArgs& InPaintArgs, const FGeometry& InGeometry, const FSlateRect& InSlateRect, FSlateWindowElementList& OutElements, int32 InLayerIndex, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled);

	FReply OnPropertyDrop(FDragDropEvent const& InDragDropEvent);

	void OnSelectionChanged(FPropertiesViewControllerItemPtr InItem, ESelectInfo::Type InSelectInfo);

	TSharedRef<ITableRow> OnGenerateTile(FPropertiesViewControllerItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	TSharedRef<SWidget> OnGetMenuContent();

	void FillNewAnimatorPropertyViewSection(UToolMenu* InToolMenu) const;

	TSharedPtr<STileView<FPropertiesViewControllerItemPtr>> ControllersTile;
	TArray<FPropertiesViewControllerItemPtr> ControllersTileSource;

	TWeakPtr<SPropertyAnimatorCoreEditorPropertiesView> ViewWeak;
	FPropertiesViewItemPtrWeak RowItemWeak;
};