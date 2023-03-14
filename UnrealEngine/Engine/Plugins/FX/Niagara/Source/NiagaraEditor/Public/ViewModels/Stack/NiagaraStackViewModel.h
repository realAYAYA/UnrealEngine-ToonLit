// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "UObject/ObjectKey.h"
#include "NiagaraStackViewModel.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraEmitterHandleViewModel;
class UNiagaraStackEntry;
class UNiagaraStackRoot;

struct FNiagaraStackViewModelOptions
{
	FNiagaraStackViewModelOptions()
		: bIncludeSystemInformation(false)
		, bIncludeEmitterInformation(false)
	{
	}

	FNiagaraStackViewModelOptions(bool bInIncludeSystemInformation, bool bInIncludeEmitterInformation)
		: bIncludeSystemInformation(bInIncludeSystemInformation)
		, bIncludeEmitterInformation(bInIncludeEmitterInformation)
	{
	}

	bool GetIncludeSystemInformation() { return bIncludeSystemInformation; }
	bool GetIncludeEmitterInformation() { return bIncludeEmitterInformation; }

private:
	bool bIncludeSystemInformation;
	bool bIncludeEmitterInformation;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackViewModel : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnChangeSearchTextExternal, FText)
	DECLARE_MULTICAST_DELEGATE(FOnExpansionChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStructureChanged, ENiagaraStructureChangedFlags);
	DECLARE_MULTICAST_DELEGATE(FOnSearchCompleted);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDataObjectChanged, TArray<UObject*> /** Changed objects */, ENiagaraDataObjectChange /** Change type */);
public:
	struct FSearchResult
	{
		TArray<UNiagaraStackEntry*> EntryPath;
		UNiagaraStackEntry::FStackSearchItem MatchingItem;
		UNiagaraStackEntry* GetEntry()
		{
			return EntryPath.Num() > 0 ? 
				EntryPath[EntryPath.Num() - 1] : 
				nullptr;
		}
	};

	struct NIAGARAEDITOR_API FTopLevelViewModel
	{
		FTopLevelViewModel(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel);

		FTopLevelViewModel(TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

		bool IsValid() const;

		UNiagaraStackEditorData* GetStackEditorData() const;

		FText GetDisplayName() const;

		bool operator==(const FTopLevelViewModel& Other)const;

		const TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;
		const TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;
		const TWeakObjectPtr<UNiagaraStackEntry> RootEntry;
	};

public:
	void InitializeWithViewModels(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel, FNiagaraStackViewModelOptions InOptions);
	void InitializeWithRootEntry(UNiagaraStackEntry* Root);

	void Finalize();

	virtual void BeginDestroy() override;

	UNiagaraStackEntry* GetRootEntry();

	TArray<UNiagaraStackEntry*>& GetRootEntryAsArray();

	FOnChangeSearchTextExternal& OnChangeSearchTextExternal();

	FOnExpansionChanged& OnExpansionChanged();
	FOnExpansionChanged& OnExpansionInOverviewChanged();
	FOnStructureChanged& OnStructureChanged();
	FOnSearchCompleted& OnSearchCompleted();
	FOnDataObjectChanged& OnDataObjectChanged();

	bool GetShowAllAdvanced() const;
	void SetShowAllAdvanced(bool bInShowAllAdvanced);

	bool GetShowOutputs() const;
	void SetShowOutputs(bool bInShowOutputs);

	bool GetShowLinkedInputs() const;
	void SetShowLinkedInputs(bool bInShowLinkedInputs);

	bool GetShowOnlyIssues() const;
	void SetShowOnlyIssues(bool bInShowOnlyIssues);

	double GetLastScrollPosition() const;
	void SetLastScrollPosition(double InLastScrollPosition);

	virtual void Tick();
	//~ stack search stuff
	void ResetSearchText();
	FText GetCurrentSearchText() const { return CurrentSearchText; };
	void OnSearchTextChanged(const FText& SearchText);
	void SetSearchTextExternal(const FText& NewSearchText);
	bool IsSearching();
	const TArray<FSearchResult>& GetCurrentSearchResults();
	int GetCurrentFocusedMatchIndex() const { return CurrentFocusedSearchMatchIndex; }
	UNiagaraStackEntry* GetCurrentFocusedEntry();
	void AddSearchScrollOffset(int NumberOfSteps);

	void OnCycleThroughIssues(TSharedPtr<FTopLevelViewModel> TopLevelToCycle);
	UNiagaraStackEntry* GetCurrentFocusedIssue() const;

	void GetPathForEntry(UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*>& EntryPath) const;

	/** Starts recursing through all entries to expand all groups and collapse all items. */
	void CollapseToHeaders();
	
	void UndismissAllIssues();
	bool HasDismissedStackIssues();

	const TArray<TSharedRef<FTopLevelViewModel>>& GetTopLevelViewModels() const;

	TSharedPtr<FTopLevelViewModel> GetTopLevelViewModelForEntry(UNiagaraStackEntry& InEntry) const;

	void Reset();
	bool HasIssues() const;
	void Refresh() { bRefreshPending = true; }
	void RequestValidationUpdate() { bValidatorUpdatePending = true; }

	bool ShouldHideDisabledModules() const;

	void InvalidateCachedParameterUsage();

private:
	/** Recursively Expands all groups and collapses all items in the stack. */
	void CollapseToHeadersRecursive(TArray<UNiagaraStackEntry*> Entries);

	struct FSearchWorkItem
	{
		TArray<UNiagaraStackEntry*> EntryPath;
		UNiagaraStackEntry* GetEntry()
		{
			return EntryPath.Num() > 0 ?
				EntryPath[EntryPath.Num() - 1] :
				nullptr;
		}
	};

	void EntryExpansionChanged();
	void EntryExpansionInOverviewChanged();
	void EntryStructureChanged(ENiagaraStructureChangedFlags Flags);
	void EntryDataObjectModified(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange ChangeType);
	void EntryRequestFullRefresh();
	void EntryRequestFullRefreshDeferred();
	void RefreshTopLevelViewModels();
	void RefreshHasIssues();
	void EmitterParentRemoved();
	/** Called by the tick function to perform partial search */
	void SearchTick();
	void GenerateTraversalEntries(UNiagaraStackEntry* Root, TArray<UNiagaraStackEntry*> ParentChain, 
		TArray<FSearchWorkItem>& TraversedArray);
	bool ItemMatchesSearchCriteria(UNiagaraStackEntry::FStackSearchItem SearchItem);
	void GeneratePathForEntry(UNiagaraStackEntry* Root, UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*> CurrentPath, TArray<UNiagaraStackEntry*>& EntryPath) const;

	void InvalidateSearchResults();
	void UpdateStackWithValidationResults();

private:
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModel;
	
	TArray<UNiagaraStackEntry*> RootEntries;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEntry> RootEntry;

	bool bExternalRootEntry;

	/** Used to forward a programmatic change in search text to a widget representing the search. The widget is responsible for binding to this delegate. */
	FOnChangeSearchTextExternal OnChangeSearchTextExternalDelegate;
	
	FOnExpansionChanged ExpansionChangedDelegate;
	FOnExpansionChanged ExpansionInOverviewChangedDelegate;
	FOnStructureChanged StructureChangedDelegate;
	FOnDataObjectChanged DataObjectChangedDelegate;

	// ~Search stuff
	FText CurrentSearchText;
	int CurrentFocusedSearchMatchIndex;
	FOnSearchCompleted SearchCompletedDelegate;
	TArray<FSearchWorkItem> ItemsToSearch;
	TArray<FSearchResult> CurrentSearchResults;
	static const double MaxSearchTime;
	bool bRestartSearch;
	bool bRefreshPending;
	bool bValidatorUpdatePending;
	bool bHasIssues;
	int32 CurrentIssueCycleIndex;
	TWeakPtr<FTopLevelViewModel> CyclingIssuesForTopLevel;

	bool bUsesTopLevelViewModels;
	TArray<TSharedRef<FTopLevelViewModel>> TopLevelViewModels;

	FNiagaraStackViewModelOptions Options;
};