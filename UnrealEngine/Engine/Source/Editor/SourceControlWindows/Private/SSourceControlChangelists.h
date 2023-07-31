// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

#include "ISourceControlProvider.h"
#include "SSourceControlCommon.h"
#include "Misc/TextFilter.h"

class FChangelistGroupTreeItem;
class SExpandableChangelistArea;
class SSearchBox;
class USourceControlSettings;

class SChangelistTree : public STreeView<FChangelistTreeItemPtr>
{
private:
	virtual void Private_SetItemSelection(FChangelistTreeItemPtr TheItem, bool bShouldBeSelected, bool bWasUserDirected = false) override;
};


/**
 * Displays the user source control change lists.
 */
class SSourceControlChangelistsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSourceControlChangelistsWidget) {}
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Set selected files */
	void SetSelectedFiles(const TArray<FString>& Filenames);

private:
	// Holds the list/state of selected and expanded items in the changelist views and file views.
	struct FExpandedAndSelectionStates
	{
		TSharedPtr<IChangelistTreeItem> SelectedChangelistNode;
		TSharedPtr<IChangelistTreeItem> SelectedUncontrolledChangelistNode;
		TArray<TSharedPtr<IChangelistTreeItem>> SelectedFileNodes;
		TSet<TSharedPtr<IChangelistTreeItem>> ExpandedTreeNodes;
		bool bShelvedFilesNodeSelected = false;
	};

private:
	TSharedRef<SChangelistTree> CreateChangelistTreeView(TArray<TSharedPtr<IChangelistTreeItem>>& ItemSources);
	TSharedRef<STreeView<FChangelistTreeItemPtr>> CreateChangelistFilesView();

	TSharedRef<ITableRow> OnGenerateRow(FChangelistTreeItemPtr InTreeItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetFileChildren(FChangelistTreeItemPtr InParent, TArray<FChangelistTreeItemPtr>& OutChildren);
	void OnGetChangelistChildren(FChangelistTreeItemPtr InParent, TArray<FChangelistTreeItemPtr>& OutChildren);
	void OnFileViewHiddenColumnsListChanged();

	EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);
	void SortFileView();

	void OnChangelistSearchTextChanged(const FText& InFilterText);
	void OnUncontrolledChangelistSearchTextChanged(const FText& InFilterText);
	void OnFileSearchTextChanged(const FText& InFilterText);
	void PopulateItemSearchStrings(const IChangelistTreeItem& Item, TArray<FString>& OutStrings);

	FReply OnFilesDragged(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);

	void RequestChangelistsRefresh();
	void RequestFileStatusRefresh(const IChangelistTreeItem& Changelist);
	void RequestFileStatusRefresh(const TSet<FString>& Pathnames);
	void OnRefresh();
	void ClearChangelistsTree();

	TSharedPtr<SWidget> OnOpenContextMenu();

	/** Returns the currently selected changelist state ptr or null in invalid cases */
	FSourceControlChangelistStatePtr GetCurrentChangelistState();
	FUncontrolledChangelistStatePtr GetCurrentUncontrolledChangelistState() const;
	FSourceControlChangelistPtr GetCurrentChangelist();
	TOptional<FUncontrolledChangelist> GetCurrentUncontrolledChangelist() const;
	FSourceControlChangelistStatePtr GetChangelistStateFromSelection();
	FSourceControlChangelistPtr GetChangelistFromSelection();

	/** Returns list of currently selected files */
	TArray<FString> GetSelectedFiles();

	/**
	 * Splits selected files between Controlled and Uncontrolled files.
	 * @param 	OutControlledFiles 		Selected source controlled files will be added to this array.
	 * @param 	OutUncontrolledFiles	Selected uncontrolled files will be added to this array.
	 */
	void GetSelectedFiles(TArray<FString>& OutControlledFiles, TArray<FString>& OutUncontrolledFiles);

	/**
	 * Splits selected files between Controlled and Uncontrolled files.
	 * @param 	OutControlledFileStates 	Selected source controlled file states will be added to this array.
	 * @param 	OutUncontrolledFileStates	Selected uncontrolled file states will be added to this array.
	 */
	void GetSelectedFileStates(TArray<FSourceControlStateRef>& OutControlledFileStates, TArray<FSourceControlStateRef>& OutUncontrolledFileStates);

	/** Returns list of currently selected shelved files */
	TArray<FString> GetSelectedShelvedFiles();

	/** Intercept Enter and Delete key presses to Submit or Delete the selected changelist (if conditions are met) */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Changelist operations */
	void OnNewChangelist();
	void OnDeleteChangelist();
	bool CanDeleteChangelist();
	bool CanDeleteChangelist(FText* OutFailureMessage);
	void OnEditChangelist();
	void OnSubmitChangelist();
	bool CanSubmitChangelist();
	bool CanSubmitChangelist(FText* OutFailureMessage);
	void OnValidateChangelist();
	bool CanValidateChangelist();

	/** Uncontrolled Changelist operations */
	void OnNewUncontrolledChangelist();
	void OnEditUncontrolledChangelist();
	bool CanEditUncontrolledChangelist();
	void OnDeleteUncontrolledChangelist();
	bool CanDeleteUncontrolledChangelist();

	/** Changelist & File operations */
	void OnRevertUnchanged();
	bool CanRevertUnchanged();
	void OnRevert();
	bool CanRevert();
	void OnShelve();

	/** Changelist & shelved files operations */
	void OnUnshelve();
	void OnDeleteShelvedFiles();

	/** Files operations */
	void OnMoveFiles();
	void OnShowHistory();
	void OnDiffAgainstDepot();
	bool CanDiffAgainstDepot();

	/** Shelved files operations */
	void OnDiffAgainstWorkspace();
	bool CanDiffAgainstWorkspace();

	/** Source control callbacks */
	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);
	void OnSourceControlStateChanged();
	void OnItemDoubleClicked(TSharedPtr<IChangelistTreeItem> Item);
	void OnChangelistSelectionChanged(TSharedPtr<IChangelistTreeItem> SelectedItem, ESelectInfo::Type SelectionType);
	void OnChangelistsStatusUpdated(const TSharedRef<ISourceControlOperation>& InOperation, ECommandResult::Type InType);

	void OnStartSourceControlOperation(TSharedRef<ISourceControlOperation> Operation, const FText& Message);
	void OnEndSourceControlOperation(const TSharedRef<ISourceControlOperation>& Operation, ECommandResult::Type InType);

	// Wrapper functions on top of the source control ones to display slow tasks for synchronous operations or toast notifications for async ones.
	void Execute(const FText& Message, const TSharedRef<ISourceControlOperation>& InOperation, TSharedPtr<ISourceControlChangelist> InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate);
	void Execute(const FText& Message, const TSharedRef<ISourceControlOperation>& InOperation, TSharedPtr<ISourceControlChangelist> InChangelist, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate);
	void Execute(const FText& Message, const TSharedRef<ISourceControlOperation>& InOperation, const EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate);
	void Execute(const FText& Message, const TSharedRef<ISourceControlOperation>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate);
	void ExecuteUncontrolledChangelistOperation(const FText& Message, const TFunction<void()>& UncontrolledChangelistTask);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SaveExpandedAndSelectionStates(FExpandedAndSelectionStates& OutStates);
	void RestoreExpandedAndSelectionStates(const FExpandedAndSelectionStates& InStates);

	TSharedRef<SWidget> MakeToolBar();
	
	/** Executes an operation to updates the changelist description of the provided changelist with a new description. */
	void EditChangelistDescription(const FText& InNewChangelistDescription, const FSourceControlChangelistStatePtr& InChangelistState);

private:
	TSharedPtr<SExpandableChangelistArea> ChangelistExpandableArea;
	TSharedPtr<SExpandableChangelistArea> UncontrolledChangelistExpandableArea;

	/** Hold the nodes displayed by the changelist tree. */
	TArray<TSharedPtr<IChangelistTreeItem>> ChangelistTreeNodes;
	TArray<TSharedPtr<IChangelistTreeItem>> UncontrolledChangelistTreeNodes;

	/** Hold the nodes displayed by the file tree. */
	TArray<TSharedPtr<IChangelistTreeItem>> FileTreeNodes;

	/** Display the changelists, uncontrolled changelists and shelved nodes. */
	TSharedPtr<SChangelistTree> ChangelistTreeView;
	TSharedPtr<SChangelistTree> UncontrolledChangelistTreeView;

	/** Display the list of files associated to the selected changelist, uncontrolled changelist or shelved node. */
	TSharedPtr<STreeView<FChangelistTreeItemPtr>> FileTreeView;
	TArray<FName> FileViewHiddenColumnsList;

	/** Source control state changed delegate handle */
	FDelegateHandle SourceControlStateChangedDelegateHandle;

	bool bShouldRefresh = false;
	bool bSourceControlAvailable = false;

	/** Files to select after refresh */
	TArray<FString> FilesToSelect;

	FName PrimarySortedColumn;
	FName SecondarySortedColumn;
	EColumnSortMode::Type PrimarySortMode = EColumnSortMode::Ascending;
	EColumnSortMode::Type SecondarySortMode = EColumnSortMode::None;

	TSharedPtr<TTextFilter<const IChangelistTreeItem&>> ChangelistTextFilter;
	TSharedPtr<TTextFilter<const IChangelistTreeItem&>> UncontrolledChangelistTextFilter;
	TSharedPtr<TTextFilter<const IChangelistTreeItem&>> FileTextFilter;
	TSharedPtr<SSearchBox> FileSearchBox;

	void StartRefreshStatus();
	void TickRefreshStatus(double InDeltaTime);
	void EndRefreshStatus();

	FText RefreshStatus;
	bool bIsRefreshing = false;
	double RefreshStatusStartSecs;

	float ChangelistAreaSize = 0.3; // [0.0f, 1.0f]
	float FileAreaSize = 0.7;
};
