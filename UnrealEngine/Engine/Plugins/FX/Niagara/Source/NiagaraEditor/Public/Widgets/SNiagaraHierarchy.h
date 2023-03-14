// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SDropTarget.h"
#include "IDetailsView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Styling/SlateTypes.h"

class UNiagaraHierarchyViewModelBase;
class UNiagaraHierarchySection;
struct FNiagaraHierarchyItemViewModelBase;
struct FNiagaraHierarchySectionViewModel;

class SNiagaraHierarchy : public SCompoundWidget
{
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateRowContentWidget, TSharedRef<FNiagaraHierarchyItemViewModelBase> HierarchyItem);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateCustomDetailsPanelNameWidget, TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem);

	SLATE_BEGIN_ARGS(SNiagaraHierarchy)
		: _bReadOnly(true)
	{}
		SLATE_ARGUMENT(bool, bReadOnly)
		SLATE_EVENT(FOnGenerateRowContentWidget, OnGenerateRowContentWidget)
		SLATE_EVENT(FOnGenerateCustomDetailsPanelNameWidget, OnGenerateCustomDetailsPanelNameWidget)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel);
	virtual ~SNiagaraHierarchy() override;
	
	void RefreshSourceView(bool bFullRefresh = false) const;
	void RefreshHierarchyView(bool bFullRefresh = false) const;
	void RefreshSectionsWidget();
	bool IsItemSelected(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) const;
	void SelectObjectInDetailsPanel(UObject* Object) const;
	
private:
	TSharedRef<ITableRow> GenerateSourceItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);
	TSharedRef<ITableRow> GenerateHierarchyItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);
private:	
	void RequestRenameSelectedItem();
	bool CanRequestRenameSelectedItem() const;

	void DeleteSelectedItems();
	bool CanDeleteSelectedItems() const;

	void OnItemAdded(TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddedItem);
	void OnSectionActivated(TSharedPtr<FNiagaraHierarchySectionViewModel> Section) const;
	void OnSelectionChanged(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, ESelectInfo::Type Type, bool bFromHierarchy) const;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** This handles the drag & drop at the root level of the hierarchy, i.e. when dragging and dropping a parameter into the "empty" hierarchy */
	FReply HandleHierarchyRootDrop(const FGeometry&, const FDragDropEvent& DragDropEvent) const;
	bool CanDropOnRoot(TSharedPtr<FDragDropOperation> DragDropOp) const;
private:
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	TSharedPtr<STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>> SourceTreeView;
	TSharedPtr<STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>> HierarchyTreeView;
	TMap<TSharedPtr<FNiagaraHierarchySectionViewModel>, TSharedPtr<class SNiagaraHierarchySection>> SectionsWidgetMap;
	TSharedPtr<class SWrapBox> SectionBox;
	TSharedPtr<IDetailsView> DetailsPanel;
	FSlateBrush LightBackgroundBrush;
	FSlateBrush RecessedBackgroundBrush;
private:
	FOnGenerateRowContentWidget OnGenerateRowContentWidget;
	FOnGenerateCustomDetailsPanelNameWidget OnGenerateCustomDetailsPanelNameWidget;
};

class SNiagaraHierarchySection : public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FOnSectionActivated, TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel)
	
	SLATE_BEGIN_ARGS(SNiagaraHierarchySection)
	{}
		SLATE_EVENT(FIsSelected, IsSelected)
		SLATE_EVENT(FOnSectionActivated, OnSectionActivated)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FNiagaraHierarchySectionViewModel> InSection, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel);
	virtual ~SNiagaraHierarchySection() override;

	bool OnCanAcceptDrop(TSharedPtr<FDragDropOperation> DragDropOperation, EItemDropZone ItemDropZone) const;
	FReply OnDroppedOn(const FGeometry&, const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const;

	void EnterEditingMode() const;

private:
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	
	TSharedRef<SWidget> OnGetMenuContent() const;

	UNiagaraHierarchySection* TryGetSectionData() const;

	FText GetText() const;
	FText GetTooltipText() const;
	void OnRenameSection(const FText& Text, ETextCommit::Type CommitType) const;
	bool OnVerifySectionRename(const FText& NewName, FText& OutTooltip) const;

	ECheckBoxState GetSectionCheckState() const;
	void OnSectionCheckChanged(ECheckBoxState NewState);
private:
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel;
private:
	FOnSectionActivated OnSectionActivatedDelegate;
};

class SNiagaraHierarchyCategory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraHierarchyCategory)
	{}
		SLATE_EVENT(FIsSelected, IsSelected)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FNiagaraHierarchyCategoryViewModel> InCategory);

	void EnterEditingMode() const;
	bool OnVerifyCategoryRename(const FText& NewName, FText& OutTooltip) const;
	
	FText GetCategoryText() const;
	void OnRenameCategory(const FText& NewText, ETextCommit::Type) const;
	
private:
	TWeakPtr<struct FNiagaraHierarchyCategoryViewModel> Category;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
};

class SNiagaraSectionDragDropTarget : public SDropTarget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSectionDragDropTarget)
		: _DropTargetArgs(SDropTarget::FArguments())
	{}
		SLATE_ARGUMENT(SDropTarget::FArguments, DropTargetArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
};
