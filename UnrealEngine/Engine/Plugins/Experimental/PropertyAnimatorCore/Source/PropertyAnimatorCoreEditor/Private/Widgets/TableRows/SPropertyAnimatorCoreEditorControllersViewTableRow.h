// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SPropertyAnimatorCoreEditorPropertiesView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class SPropertyAnimatorCoreEditorControllersView;

class SPropertyAnimatorCoreEditorControllersViewTableRow : public SMultiColumnTableRow<FControllersViewItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorControllersViewTableRow) {}
	SLATE_END_ARGS()

	virtual ~SPropertyAnimatorCoreEditorControllersViewTableRow() override;

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView,
		TSharedPtr<SPropertyAnimatorCoreEditorControllersView> InView, FControllersViewItemPtr InItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

protected:
	int32 OnPropertyPaintDropIndicator(EItemDropZone InDropZone, const FPaintArgs& InPaintArgs, const FGeometry& InGeometry, const FSlateRect& InSlateRect, FSlateWindowElementList& OutElements, int32 InLayerIndex, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled);

	FReply OnPropertyDrop(FDragDropEvent const& InDragDropEvent);

	TOptional<EItemDropZone> OnPropertyCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FControllersViewItemPtr InItem);

	void OnGlobalSelectionChanged();

	void OnControllerRenameRequested(UPropertyAnimatorCoreBase* InController);

	/** Before commiting new name, place verify rules here */
	bool OnVerifyControllerTextChanged(const FText& InText, FText& OutError) const;

	/** When we commit the editable text box */
	void OnControllerTextCommitted(const FText& InText, ETextCommit::Type InType) const;

	/** Used to enter renaming mode */
	FReply OnControllerBoxDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InEvent) const;

	void BeginRenamingOperation() const;

	void EndRenamingOperation() const;

	TSharedPtr<SEditableTextBox> ControllerEditableTextBox;
	TWeakPtr<SPropertyAnimatorCoreEditorControllersView> ViewWeak;
	FControllersViewItemPtrWeak RowItemWeak;
};