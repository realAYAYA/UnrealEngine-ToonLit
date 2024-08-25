// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SDropTarget.h"
#include "IDetailsView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Styling/SlateTypes.h"
#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"

class UNiagaraHierarchyViewModelBase;
class UNiagaraHierarchySection;
struct FNiagaraHierarchyItemViewModelBase;
struct FNiagaraHierarchySectionViewModel;

class SNiagaraHierarchySection : public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FOnSectionActivated, TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel)
	
	SLATE_BEGIN_ARGS(SNiagaraHierarchySection)
		: _bForbidDropOn(false)
		{}
		SLATE_ATTRIBUTE(ECheckBoxState, IsSectionActive)
		SLATE_EVENT(FOnSectionActivated, OnSectionActivated)
		SLATE_ARGUMENT(bool, bForbidDropOn)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FNiagaraHierarchySectionViewModel> InSection, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel);
	virtual ~SNiagaraHierarchySection() override;

	bool OnCanAcceptDrop(TSharedPtr<FDragDropOperation> DragDropOperation, EItemDropZone ItemDropZone) const;
	FReply OnDroppedOn(const FGeometry&, const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const;

	void TryEnterEditingMode() const;

	TSharedPtr<struct FNiagaraHierarchySectionViewModel> GetSectionViewModel();
private:
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	UNiagaraHierarchySection* TryGetSectionData() const;

	FText GetText() const;
	FText GetTooltipText() const;
	void OnRenameSection(const FText& Text, ETextCommit::Type CommitType) const;
	bool OnVerifySectionRename(const FText& NewName, FText& OutTooltip) const;

	bool IsSectionSelected() const;
	bool IsSectionReadOnly() const;
	ECheckBoxState GetSectionCheckState() const;
	void OnSectionCheckChanged(ECheckBoxState NewState);
	EActiveTimerReturnType ActivateSectionIfDragging(double CurrentTime, float DeltaTime) const;
private:
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SCheckBox> CheckBox;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel;
private:
	TAttribute<ECheckBoxState> IsSectionActive;
	FOnSectionActivated OnSectionActivatedDelegate;
	bool bForbidDropOn = false;
	mutable bool bDraggedOn = false;
};

class SNiagaraHierarchy : public SCompoundWidget, public FGCObject, public FNotifyHook
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

	void RefreshSourceItems();
	void RefreshAllViews(bool bFullRefresh = false);
	void RequestRefreshAllViewsNextFrame(bool bFullRefresh = false);
	void RefreshSourceView(bool bFullRefresh = false) const;
	void RequestRefreshSourceViewNextFrame(bool bFullRefresh = false);
	void RefreshHierarchyView(bool bFullRefresh = false) const;
	void RequestRefreshHierarchyViewNextFrame(bool bFullRefresh = false);
	void RefreshSectionsView();
	void RequestRefreshSectionsViewNextFrame();

	void NavigateToHierarchyItem(FNiagaraHierarchyIdentity Identity) const;
	void NavigateToHierarchyItem(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) const;
	bool IsItemSelected(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) const;
	
private:
	// need to do this to enable focus so we can handle shortcuts
	virtual bool SupportsKeyboardFocus() const override { return true; }
	
	TSharedRef<ITableRow> GenerateSourceItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);
	TSharedRef<ITableRow> GenerateHierarchyItemRow(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);

	bool FilterForSourceSection(TSharedPtr<const FNiagaraHierarchyItemViewModelBase> ItemViewModel) const;
private:
	void Reinitialize();

	void BindToHierarchyRootViewModel();
	void UnbindFromHierarchyRootViewModel() const;
	
	/** Source items reflect the base, unedited status of items to edit into a hierarchy */
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetSourceItems() const;
	
	bool IsDetailsPanelEditingAllowed() const;
	
	void RequestRenameSelectedItem();
	bool CanRequestRenameSelectedItem() const;

	void ClearSourceItems() const;

	void DeleteItems(TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> ItemsToDelete) const;
	void DeleteSelectedHierarchyItems() const;
	bool CanDeleteSelectedHierarchyItems() const;

	void DeleteActiveSection() const;
	bool CanDeleteActiveSection() const;

	void OnItemAdded(TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddedItem);
	void OnHierarchySectionActivated(TSharedPtr<FNiagaraHierarchySectionViewModel> Section);
	void OnSourceSectionActivated(TSharedPtr<FNiagaraHierarchySectionViewModel> Section);
	void OnHierarchySectionAdded(TSharedPtr<FNiagaraHierarchySectionViewModel> AddedSection);
	void OnHierarchySectionDeleted(TSharedPtr<FNiagaraHierarchySectionViewModel> DeletedSection);

	void SetActiveSourceSection(TSharedPtr<struct FNiagaraHierarchySectionViewModel>);
	TSharedPtr<FNiagaraHierarchySectionViewModel> GetActiveSourceSection() const;
	UNiagaraHierarchySection* GetActiveSourceSectionData() const;
	
	void OnSelectionChanged(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem, ESelectInfo::Type Type, bool bFromHierarchy) const;

	void RunSourceSearch();
	void OnSourceSearchTextChanged(const FText& Text);
	void OnSourceSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	void OnSearchButtonClicked(SSearchBox::SearchDirection SearchDirection);

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	TSharedPtr<SWidget> SummonContextMenuForSelectedRows(bool bFromHierarchy) const;

	struct FSearchItem
	{
		TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> Path;

		TSharedPtr<FNiagaraHierarchyItemViewModelBase> GetEntry() const
		{
			return Path.Num() > 0 ? 
				Path[Path.Num() - 1] : 
				nullptr;
		}

		bool operator==(const FSearchItem& Item) const
		{
			return Path == Item.Path;
		}
	};

	/** This will recursively generated parent chain paths for all items within the given root. Used for expansion purposes. */
	void GenerateSearchItems(TSharedRef<FNiagaraHierarchyItemViewModelBase> Root, TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> ParentChain, TArray<FSearchItem>& OutSearchItems);
	void ExpandSourceSearchResults();
	void SelectNextSourceSearchResult();
	void SelectPreviousSourceSearchResult();
	TOptional<SSearchBox::FSearchResultData> GetSearchResultData() const;
	
	FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults CanDropOnRoot(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem) const;

	/** Callback functions for the root widget */
	FReply HandleHierarchyRootDrop(const FGeometry&, const FDragDropEvent& DragDropEvent) const;
	bool OnCanDropOnRoot(TSharedPtr<FDragDropOperation> DragDropOperation) const;
	void OnRootDragEnter(const FDragDropEvent& DragDropEvent) const;
	void OnRootDragLeave(const FDragDropEvent& DragDropEvent) const;
	FSlateColor GetRootIconColor() const;

	virtual FString GetReferencerName() const override; 
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
private:
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	
	TArray<FSearchItem> SourceSearchResults;
	TOptional<FSearchItem> FocusedSearchResult;

	mutable TWeakPtr<FNiagaraHierarchyItemViewModelBase> SelectedDetailsPanelItemViewModel;

private:
	TObjectPtr<UNiagaraHierarchyRoot> SourceRoot = nullptr;
	TSharedPtr<FNiagaraHierarchyRootViewModel> SourceRootViewModel;
	TSharedPtr<STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>> SourceTreeView;
	TSharedPtr<STreeView<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>> HierarchyTreeView;
	TWeakPtr<struct FNiagaraHierarchySectionViewModel> ActiveSourceSection;
	TSharedPtr<class SWrapBox> SourceSectionBox;
	TSharedPtr<class SWrapBox> HierarchySectionBox;
	TSharedPtr<SSearchBox> SourceSearchBox;
	TSharedPtr<IDetailsView> DetailsPanel;
private:
	FOnGenerateRowContentWidget OnGenerateRowContentWidget;
	FOnGenerateCustomDetailsPanelNameWidget OnGenerateCustomDetailsPanelNameWidget;
	TSharedPtr<FActiveTimerHandle> RefreshHierarchyViewNextFrameHandle;
	TSharedPtr<FActiveTimerHandle> RefreshSourceViewNextFrameHandle;
	TSharedPtr<FActiveTimerHandle> RefreshSectionsViewNextFrameHandle;
};

class SNiagaraHierarchyCategory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraHierarchyCategory)
	{}
		SLATE_EVENT(FIsSelected, IsSelected)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FNiagaraHierarchyCategoryViewModel> InCategoryViewModel);

	void EnterEditingMode() const;
	bool OnVerifyCategoryRename(const FText& NewName, FText& OutTooltip) const;
	
	FText GetCategoryText() const;
	void OnRenameCategory(const FText& NewText, ETextCommit::Type) const;
	
private:
	TWeakPtr<struct FNiagaraHierarchyCategoryViewModel> CategoryViewModel;
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

	void Construct(const FArguments& InArgs, TSharedPtr<FNiagaraHierarchySectionViewModel> OwningSection, EItemDropZone ItemDropZone);
	
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

private:
	TSharedPtr<FNiagaraHierarchySectionViewModel> OwningSection;
	/** The drop zone this drop target represents */
	EItemDropZone DropZone = EItemDropZone::OntoItem;
};
