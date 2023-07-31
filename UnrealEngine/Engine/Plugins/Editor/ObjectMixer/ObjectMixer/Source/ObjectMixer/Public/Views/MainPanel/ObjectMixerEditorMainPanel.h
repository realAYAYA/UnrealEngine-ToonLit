// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"

#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

class FObjectMixerEditorListFilter_Collection;
class UObjectMixerEditorSerializedData;
struct FObjectMixerEditorListRow;
class IObjectMixerEditorListFilter;
class UObjectMixerObjectFilter;
class FObjectMixerEditorList;
class SObjectMixerEditorMainPanel;

DECLARE_MULTICAST_DELEGATE(FOnPreFilterChange)
DECLARE_MULTICAST_DELEGATE(FOnPostFilterChange)

class OBJECTMIXEREDITOR_API FObjectMixerEditorMainPanel : public TSharedFromThis<FObjectMixerEditorMainPanel>
{
public:
	FObjectMixerEditorMainPanel(const FName InModuleName)
	: ModuleName(InModuleName)
	{}

	~FObjectMixerEditorMainPanel() = default;

	void Init();

	TSharedRef<SWidget> GetOrCreateWidget();

	void RegenerateListModel();

	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing items.
	 */
	void RequestRebuildList() const;

	/**
	 * Refresh filters and sorting.
	 * Useful for when the list state has gone stale but the variable count has not changed.
	 */
	void RefreshList() const;

	void RequestSyncEditorSelectionToListSelection() const;

	TWeakPtr<FObjectMixerEditorList> GetEditorListModel() const
	{
		return EditorListModel;
	}

	void RebuildCollectionSelector();

	FText GetSearchTextFromSearchInputField() const;
	FString GetSearchStringFromSearchInputField() const;

	void OnClassSelectionChanged(UClass* InNewClass);
	TObjectPtr<UClass> GetClassSelection() const;
	bool IsClassSelected(UClass* InNewClass) const;

	UObjectMixerObjectFilter* GetObjectFilter();

	void CacheObjectFilterObject();

	/**
	 * Get the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode()
	{
		return TreeViewMode;
	}
	/**
	 * Set the style of the tree (flat list or hierarchy)
	 */
	void SetTreeViewMode(EObjectMixerTreeViewMode InViewMode)
	{
		TreeViewMode = InViewMode;
		RequestRebuildList();
	}

	/**
	 * Returns result from Filter->GetObjectClassesToFilter.
	 */
	TSet<UClass*> GetObjectClassesToFilter()
	{
		if (const UObjectMixerObjectFilter* Filter = GetObjectFilter())
		{
			return Filter->GetObjectClassesToFilter();
		}
		
		return {};
	}

	/**
	 * Returns result from Filter->GetObjectClassesToPlace.
	 */
	TSet<TSubclassOf<AActor>> GetObjectClassesToPlace()
	{
		TSet<TSubclassOf<AActor>> ReturnValue;

		if (const UObjectMixerObjectFilter* Filter = GetObjectFilter())
		{
			ReturnValue = Filter->GetObjectClassesToPlace();
		}
		
		return ReturnValue;
	}

	const TArray<TSharedRef<IObjectMixerEditorListFilter>>& GetListFilters() const;
	TArray<TWeakPtr<IObjectMixerEditorListFilter>> GetWeakActiveListFiltersSortedByName() const;

	/**
	 * Get the rows that have solo visibility. All other rows should be set to temporarily invisible in editor.
	 */
	TSet<TWeakPtr<FObjectMixerEditorListRow>> GetSoloRows()
	{
		return SoloRows;
	}

	/**
	 * Add a row that has solo visibility. This does not set temporary editor invisibility for other rows.
	 */
	void AddSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
	{
		SoloRows.Add(InRow);
	}

	/**
	 * Remove a row that does not have solo visibility. This does not set temporary editor invisibility for other rows.
	 */
	void RemoveSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
	{
		SoloRows.Remove(InRow);
	}

	/**
	 * Clear the rows that have solo visibility. This does not remove temporary editor invisibility for other rows.
	 */
	void ClearSoloRows()
	{
		SoloRows.Empty();
	}

	TSubclassOf<UObjectMixerObjectFilter> GetObjectFilterClass() const
	{
		return ObjectFilterClass;
	}

	void SetObjectFilterClass(UClass* InObjectFilterClass)
	{
		if (ensureAlwaysMsgf(InObjectFilterClass->IsChildOf(UObjectMixerObjectFilter::StaticClass()), TEXT("%hs: Class '%s' is not a child of UObjectMixerObjectFilter."), __FUNCTION__, *InObjectFilterClass->GetName()))
		{
			ObjectFilterClass = InObjectFilterClass;
			CacheObjectFilterObject();
			RequestRebuildList();
		}
	}

	FName GetModuleName() const
	{
		return ModuleName;
	}

	// User Collections

	/**
	 * Get a pointer to the UObjectMixerEditorSerializedData object along with the name of the filter represented by this MainPanel instance.
	 */
	UObjectMixerEditorSerializedData* GetSerializedDataOutputtingFilterName(FName& OutFilterName) const;
	bool RequestAddObjectsToCollection(const FName& CollectionName, const TSet<FSoftObjectPath>& ObjectsToAdd) const;
	bool RequestRemoveObjectsFromCollection(const FName& CollectionName, const TSet<FSoftObjectPath>& ObjectsToRemove) const;
	bool RequestRemoveCollection(const FName& CollectionName) const;
	bool RequestDuplicateCollection(const FName& CollectionToDuplicateName, FName& DesiredDuplicateName) const;
	bool RequestReorderCollection(const FName& CollectionToMoveName, const FName& CollectionInsertBeforeName) const;
	bool RequestRenameCollection(const FName& CollectionNameToRename, const FName& NewCollectionName) const;
	bool DoesCollectionExist(const FName& CollectionName) const;
	bool IsObjectInCollection(const FName& CollectionName, const FSoftObjectPath& InObject) const;
	TSet<FName> GetCollectionsForObject(const FSoftObjectPath& InObject) const;
	TArray<FName> GetAllCollectionNames() const;

	/**
	 * Returns the collections selected by the user. If the set is empty, consider "All" collections to be selected.
	 */
	TSet<TSharedRef<FObjectMixerEditorListFilter_Collection>> GetCurrentCollectionSelection() const;

	FOnPreFilterChange OnPreFilterChange;
	FOnPostFilterChange OnPostFilterChange;

private:

	TSharedPtr<SObjectMixerEditorMainPanel> MainPanelWidget;

	TSharedPtr<FObjectMixerEditorList> EditorListModel;

	TStrongObjectPtr<UObjectMixerObjectFilter> ObjectFilterPtr;

	/**
	 * The class used to generate property edit columns
	 */
	TSubclassOf<UObjectMixerObjectFilter> ObjectFilterClass;

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode TreeViewMode = EObjectMixerTreeViewMode::Folders;

	TSet<TWeakPtr<FObjectMixerEditorListRow>> SoloRows = {};

	FName ModuleName = NAME_None;
};
