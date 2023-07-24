// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorList.h"
#include "ObjectMixerEditorListRowData.h"
#include "ObjectMixerEditorSerializedData.h"

#include "SSceneOutliner.h"

#include "SObjectMixerEditorList.generated.h"

class FObjectMixerEditorList;
class FObjectMixerOutlinerMode;
class SBox;
class SComboButton;
class SSearchBox;
class SHeaderRow;
class SWrapBox;

UENUM()
enum class EListViewColumnType
{
	BuiltIn,
	PropertyGenerated
};

USTRUCT()
struct FObjectMixerSceneOutlinerColumnInfo
{
	GENERATED_BODY()
	
	FProperty* PropertyRef = nullptr;
	
	UPROPERTY()
	FName PropertyName = NAME_None;

	UPROPERTY()
	FName ColumnID = NAME_None;

	UPROPERTY()
	FText PropertyDisplayText = FText::GetEmpty();

	UPROPERTY()
	EListViewColumnType PropertyType = EListViewColumnType::PropertyGenerated;

	UPROPERTY()
	FName PropertyCategoryName = NAME_None;

	UPROPERTY()
	bool bIsDesiredForDisplay = false;

	UPROPERTY()
	bool bCanBeHidden = true;

	UPROPERTY()
	bool bCanBeSorted = false;

	UPROPERTY()
	bool bUseFixedWidth = false;

	UPROPERTY()
	float FixedWidth = 25.0f;

	UPROPERTY()
	float FillWidth = 1.0f;
};

struct FObjectMixerEditorListRowData;

class OBJECTMIXEREDITOR_API SObjectMixerEditorList : public SSceneOutliner
{

public:
	
	// Columns
	static const FName ItemNameColumnName;
	static const FName EditorVisibilityColumnName;
	static const FName EditorVisibilitySoloColumnName;

	void Construct(const FArguments& InArgs, TSharedRef<FObjectMixerEditorList> ListModel);

	virtual ~SObjectMixerEditorList() {}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode();
	/**
	 * Determine the style of the tree (flat list or hierarchy)
	 */
	void SetTreeViewMode(EObjectMixerTreeViewMode InViewMode);

	const TSet<FName>& GetSelectedCollections();
	UE_NODISCARD bool IsCollectionSelected(const FName& CollectionName);
	
	void RebuildCollectionSelector();

	bool RequestRemoveCollection(const FName& CollectionName);
	bool RequestDuplicateCollection(const FName& CollectionToDuplicateName, FName& DesiredDuplicateName) const;
	bool RequestRenameCollection(const FName& CollectionNameToRename, const FName& NewCollectionName);
	bool DoesCollectionExist(const FName& CollectionName) const;
	
	void OnCollectionCheckedStateChanged(bool bShouldBeChecked, FName CollectionName);
	ECheckBoxState GetCollectionCheckedState(FName CollectionName) const;

	FObjectMixerOutlinerMode* GetCastedMode() const;
	UWorld* GetWorld() const;

	TWeakPtr<FObjectMixerEditorList> GetListModelPtr() const
	{
		return ListModelPtr;
	}

	/**
	 * Refresh filters and sorting.
	 * Useful for when the list state has gone stale but the variable count has not changed.
	 */
	void RefreshList();

	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing items.
	 */
	void RequestRebuildList(const FString& InItemToScrollTo = "");

	/** Called when the Rename command is executed from the UI or hotkey. */
	void OnRenameCommand();

	void AddToPendingPropertyPropagations(const FObjectMixerEditorListRowData::FPropertyPropagationInfo& InPropagationInfo);

	UE_NODISCARD TArray<TSharedPtr<ISceneOutlinerTreeItem>> GetSelectedTreeViewItems() const;
	int32 GetSelectedTreeViewItemCount() const;

	void SetTreeViewItemSelected(TSharedRef<ISceneOutlinerTreeItem> Item, const bool bNewSelected);

	bool IsTreeViewItemSelected(TSharedPtr<ISceneOutlinerTreeItem> Item);

	UE_NODISCARD int32 GetTreeViewItemCount() const
	{
		return GetTree().GetItems().Num();
	}

	TSet<TSharedPtr<ISceneOutlinerTreeItem>> GetTreeRootItems() const;
	TSet<TWeakPtr<ISceneOutlinerTreeItem>> GetWeakTreeRootItems() const;
	
	TSet<TSharedPtr<ISceneOutlinerTreeItem>> GetSoloRows() const;
	void ClearSoloRows();

	/** Returns true if at least one row is set to Solo. */
	bool IsListInSoloState() const;

	/**
	 * Determines whether rows' objects should be temporarily hidden in editor based on each row's visibility rules,
	 * then sets each object's visibility in editor.
	 */
	void EvaluateAndSetEditorVisibilityPerRow();

	bool IsTreeViewItemExpanded(const TSharedPtr<ISceneOutlinerTreeItem>& Row) const;
	void SetTreeViewItemExpanded(const TSharedPtr<ISceneOutlinerTreeItem>& RowToExpand, const bool bNewExpansion) const;

	void PropagatePropertyChangesToSelectedRows();

	// Columns

	FObjectMixerSceneOutlinerColumnInfo* GetColumnInfoByPropertyName(const FName& InPropertyName);
	TSharedRef<SWidget> GenerateHeaderRowContextMenu() const;

	/* Begin SSceneOutliner Interface */
	virtual void CustomAddToToolbar(TSharedPtr<class SHorizontalBox> Toolbar) override;
	/* End SSceneOutliner Interface */

protected:

	/** A reference to the struct that controls this widget */
	TWeakPtr<FObjectMixerEditorList> ListModelPtr;

	// User Collections
	
	TSharedPtr<SWrapBox> CollectionSelectorBox;

	bool bIsRebuildRequested = false;
	
	TSet<FObjectMixerEditorListRowData::FPropertyPropagationInfo> PendingPropertyPropagations;

	TArray<FObjectMixerSceneOutlinerColumnInfo> HeaderColumnInfos;

	TWeakPtr<SHorizontalBox> ToolbarPtr;

	bool CanCreateFolder() const;
	TSharedRef<SWidget> OnGenerateAddObjectButtonMenu() const;
	
	/** Disable all collection filters except CollectionToEnableName */
	void SetSingleCollectionSelection(const FName& CollectionToEnableName = UObjectMixerEditorSerializedData::AllCollectionName);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OnActorSpawnedOrDestroyed(AActor* Object)
	{
		RequestRebuildList();
	}
	
	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing items.
	 */
	void RebuildList();

	void InsertCollectionSelector();

	/**
	 * Only adds properties that pass a series of tests, including having only one unique entry in the column list array.
	 * @param bForceIncludeProperty If true, only Skiplist and Uniqueness tests will be checked, bypassing class, blueprint editability and other requirements.
	 * @param PropertySkipList These property names will be skipped when they are encountered in the iteration.
	 */
	bool AddUniquePropertyColumnInfo(
		FProperty* Property,
		const bool bForceIncludeProperty = false,
		const TSet<FName>& PropertySkipList = {}
	);

	FText GetLevelColumnName() const;
	void CreateActorTextInfoColumns(FSceneOutlinerInitializationOptions& OutInitOptions);
	void SetupColumns(FSceneOutlinerInitializationOptions& OutInitOptions);
};
