// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorListRowData.h"

#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

class FObjectMixerEditorModule;
class SObjectMixerEditorList;
class UObjectMixerEditorSerializedData;

DECLARE_MULTICAST_DELEGATE(FOnPreFilterChange)
DECLARE_MULTICAST_DELEGATE(FOnPostFilterChange)


class OBJECTMIXEREDITOR_API FObjectMixerEditorList : public TSharedFromThis<FObjectMixerEditorList>, public FGCObject
{
public:

	FObjectMixerEditorList(const FName InModuleName);

	virtual ~FObjectMixerEditorList();

	void Initialize();
	
	void RegisterAndMapContextMenuCommands();

	/** Creates a new widget only if one does not already exist. Otherwise returns the existing widget. */
	TSharedRef<SWidget> GetOrCreateWidget();

	/** Creates a new widget, replacing the existing one's pointer. Not useful to call alone, use RequestRegenerateListWidget */
	TSharedRef<SWidget> CreateWidget();

	/** Calls back to the module to replace the widget in the dock tab. Call when columns or the object filters have changed. */
	void RequestRegenerateListWidget();

	/** Rebuild the list items and refresh the list. Call when adding or removing items. */
	void RequestRebuildList() const;

	/**
	 * Refresh list filters and sorting.
	 * Useful for when the list state has gone stale but the variable count has not changed.
	 */
	void RefreshList() const;

	void BuildPerformanceCache();

	bool ShouldShowTransientObjects() const;

	/** Called when the Rename command is executed from the UI or hotkey. */
	void OnRenameCommand();
	
	void RebuildCollectionSelector();

	void SetDefaultFilterClass(UClass* InNewClass);
	bool IsClassSelected(UClass* InClass) const;

	const TArray<TObjectPtr<UObjectMixerObjectFilter>>& GetObjectFilterInstances();

	const UObjectMixerObjectFilter* GetMainObjectFilterInstance();

	void CacheObjectFilterInstances();

	/** Get the style of the tree (flat list or hierarchy) */
	EObjectMixerTreeViewMode GetTreeViewMode()
	{
		return TreeViewMode;
	}
	/** Set the style of the tree (flat list or hierarchy)  */
	void SetTreeViewMode(EObjectMixerTreeViewMode InViewMode)
	{
		TreeViewMode = InViewMode;
		RequestRebuildList();
	}

	/**
	 * Force returns result from Filter->ForceGetObjectClassesToFilter.
	 * Generally, you want to get this from the public performance cache.
	 */
	TSet<UClass*> ForceGetObjectClassesToFilter();

	/**
	 * Force returns result from Filter->ForceGetObjectClassesToPlace.
	 * Generally, you want to get this from the public performance cache.
	 */
	TSet<TSubclassOf<AActor>> ForceGetObjectClassesToPlace();

	const TArray<TSubclassOf<UObjectMixerObjectFilter>>& GetObjectFilterClasses() const
	{
		return ObjectFilterClasses;
	}

	void AddObjectFilterClass(UClass* InObjectFilterClass, const bool bShouldRebuild = true);

	void RemoveObjectFilterClass(UClass* InObjectFilterClass, const bool bCacheAndRebuild = true);

	void ResetObjectFilterClasses(const bool bCacheAndRebuild = true)
	{
		ObjectFilterClasses.Empty(ObjectFilterClasses.Num());

		if (bCacheAndRebuild)
		{
			CacheAndRebuildFilters();
		}
	}

	void CacheAndRebuildFilters(const bool bShouldRegenerateWidget = false)
	{
		CacheObjectFilterInstances();

		if (bShouldRegenerateWidget)
		{
			RequestRegenerateListWidget();
			return;
		}
		
		RequestRebuildList();
	}

	/** Used as a way to differentiate different subclasses of the Object Mixer module */
	FName GetModuleName() const
	{
		return ModuleName;
	}

	FObjectMixerEditorModule* GetModulePtr() const;

	// User Collections

	/** Get a pointer to the UObjectMixerEditorSerializedData object along with the name of the filter represented by this ListModel instance. */
	UObjectMixerEditorSerializedData* GetSerializedData() const;
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
	 * This is the filter class used to initialize the ListModel.
	 * This filter class cannot be turned off by the end user.
	 */
	const TSubclassOf<UObjectMixerObjectFilter>& GetDefaultFilterClass() const;

	void OnPostFilterChange();

	FOnPreFilterChange OnPreFilterChangeDelegate;
	FOnPostFilterChange OnPostFilterChangeDelegate;

	TSharedPtr<FUICommandList> ObjectMixerElementEditCommands;
	TSharedPtr<FUICommandList> ObjectMixerFolderEditCommands;

	void FlushWidget();

	[[nodiscard]] TArray<TSharedPtr<ISceneOutlinerTreeItem>> GetSelectedTreeViewItems() const;
	int32 GetSelectedTreeViewItemCount() const;

	TSet<TSharedPtr<ISceneOutlinerTreeItem>> GetSoloRows() const;
	void ClearSoloRows();

	/** Returns true if at least one row is set to Solo. */
	bool IsListInSoloState() const;

	/**
	 * Determines whether rows' objects should be temporarily hidden in editor based on each row's visibility rules,
	 * then sets each object's visibility in editor.
	 */
	void EvaluateAndSetEditorVisibilityPerRow();
	
	/** Represents the collections the user has selected in the UI. If empty, "All" is considered as selected. */
	[[nodiscard]] const TSet<FName>& GetSelectedCollections() const;
	[[nodiscard]] bool IsCollectionSelected(const FName& CollectionName) const;
	void SetSelectedCollections(const TSet<FName> InSelectedCollections);
	void SetCollectionSelected(const FName& CollectionName, const bool bNewSelected);

	// Performance cache
	TSet<UClass*> ObjectClassesToFilterCache;
	TSet<FName> ColumnsToShowByDefaultCache;
	TSet<FName> ColumnsToExcludeCache;
	TSet<FName> ForceAddedColumnsCache;
	EObjectMixerInheritanceInclusionOptions PropertyInheritanceInclusionOptionsCache = EObjectMixerInheritanceInclusionOptions::None;
	bool bShouldIncludeUnsupportedPropertiesCache = false;
	bool bShouldShowTransientObjectsCache = false;

protected:
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector )  override
	{
		Collector.AddReferencedObjects(ObjectFilterInstances);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FObjectMixerEditorList");
	}

	/** Represents the collections the user has selected in the UI. If empty, "All" is considered as selected. */
	TSet<FName> SelectedCollections;

	TSharedPtr<SObjectMixerEditorList> ListWidget;
	
	TArray<TObjectPtr<UObjectMixerObjectFilter>> ObjectFilterInstances;

	/**
	 * The classes used to generate property edit columns.
	 * Using an array rather than a set because the first class is considered the 'Main' class which determines some filter behaviours.
	 */
	TArray<TSubclassOf<UObjectMixerObjectFilter>> ObjectFilterClasses;

	/**
	 * If set, this is the filter class used to initialize the ListModel.
	 * This filter class cannot be turned off by the end user.
	 */
	TSubclassOf<UObjectMixerObjectFilter> DefaultFilterClass;

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode TreeViewMode = EObjectMixerTreeViewMode::Folders;

	FName ModuleName = NAME_None;
	
	FDelegateHandle OnBlueprintFilterCompiledHandle;
};
