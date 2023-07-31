// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Layout/Visibility.h"
#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"
#include "PropertyHandle.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

class SObjectMixerEditorList;
class UObjectMixerObjectFilter;

struct FSlateBrush;

struct FObjectMixerEditorListRow;
typedef TSharedPtr<FObjectMixerEditorListRow> FObjectMixerEditorListRowPtr;

class FObjectMixerListRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FObjectMixerListRowDragDropOp, FDecoratedDragDropOp)

	/** The item being dragged and dropped */
	TArray<FObjectMixerEditorListRowPtr> DraggedItems;

	/** Constructs a new drag/drop operation */
	static TSharedRef<FObjectMixerListRowDragDropOp> New(const TArray<FObjectMixerEditorListRowPtr>& InItems);
};

struct OBJECTMIXEREDITOR_API FObjectMixerEditorListRow final : TSharedFromThis<FObjectMixerEditorListRow>
{
	enum EObjectMixerEditorListRowType : uint8
	{
		None = 0,
		Folder, // Usually an Outliner folder
		ContainerObject, // Usually an actor that contains a matching subobject or is the attach parent of a matching actor
		MatchingContainerObject, // An Actor that is a matching object and contains matching subobjects 
		MatchingObject // The object that has the properties we wish to affect
	};

	~FObjectMixerEditorListRow();

	bool operator==(const TSharedPtr<FObjectMixerEditorListRow>& Other) const
	{
		const UObject* ThisObject = GetObject();
		const UObject* ThatObject = Other->GetObject();

		const bool bAreObjectsEqual = ThisObject == ThatObject;

		// If both are null, they're folders. Check display names.
		if (bAreObjectsEqual && !ThisObject)
		{
			return GetDisplayNameOverride().EqualTo(Other->GetDisplayNameOverride());
		}
		
		return bAreObjectsEqual;
	}

	void FlushReferences();
	
	FObjectMixerEditorListRow(
		const TWeakObjectPtr<UObject> InObject, const EObjectMixerEditorListRowType InRowType, 
		const TSharedRef<SObjectMixerEditorList>& InListView, const FText& InDisplayNameOverride = FText::GetEmpty())
	: ObjectRef(InObject)
	, RowType(InRowType)
	, ListViewPtr(InListView)
	, DisplayNameOverride(InDisplayNameOverride)
	{}

	FObjectMixerEditorListRow(
		const FName InFolderPath, const EObjectMixerEditorListRowType InRowType, 
		const TSharedRef<SObjectMixerEditorList>& InListView, const FText& InDisplayNameOverride = FText::GetEmpty())
	: FolderPath(InFolderPath)
	, RowType(InRowType)
	, ListViewPtr(InListView)
	, DisplayNameOverride(InDisplayNameOverride)
	{}

	[[nodiscard]] UObject* GetObject() const
	{
		if (ObjectRef.IsValid())
		{
			return ObjectRef.Get();
		}

		return nullptr;
	}

	[[nodiscard]] FName GetFolderPath() const
	{
		return FolderPath;
	}

	UObjectMixerObjectFilter* GetObjectFilter() const;

	bool IsObjectRefInCollection(const FName& CollectionName) const;

	[[nodiscard]] EObjectMixerEditorListRowType GetRowType() const;
	void SetRowType(EObjectMixerEditorListRowType InNewRowType);

	/** If this is a hybrid row, return the index of the child row used to hybrid with. */
	[[nodiscard]] int32 GetOrFindHybridRowIndex();
	/** If this is a hybrid row, return the child row used to hybrid with. */
	[[nodiscard]] FObjectMixerEditorListRowPtr GetHybridChild();

	[[nodiscard]] int32 GetSortOrder() const;
	void SetSortOrder(const int32 InNewOrder);

	TWeakPtr<FObjectMixerEditorListRow> GetDirectParentRow() const;
	void SetDirectParentRow(const TWeakPtr<FObjectMixerEditorListRow>& InDirectParentRow);
	
	/** bHasGeneratedChildren must be true to get actual children. */
	[[nodiscard]] const TArray<FObjectMixerEditorListRowPtr>& GetChildRows() const;
	/** bHasGeneratedChildren must be true to get an accurate value. */
	[[nodiscard]] int32 GetChildCount() const;
	void SetChildRows(const TArray<FObjectMixerEditorListRowPtr>& InChildRows);
	void AddToChildRows(const FObjectMixerEditorListRowPtr& InRow);
	void InsertChildRowAtIndex(const FObjectMixerEditorListRowPtr& InRow, const int32 AtIndex = 0);

	[[nodiscard]] bool GetIsTreeViewItemExpanded();
	void SetIsTreeViewItemExpanded(const bool bNewExpanded);

	[[nodiscard]] bool GetShouldExpandAllChildren() const;
	void SetShouldExpandAllChildren(const bool bNewShouldExpandAllChildren);

	/**
	 *Individual members of InTokens will be considered "AnyOf" or "OR" searches. If SearchTerms contains any individual member it will match.
	 *Members will be tested for a space character (" "). If a space is found, a subsearch will be run.
	 *This subsearch will be an "AllOf" or "AND" type search in which all strings, separated by a space, must be found in the search terms.
	 */
	bool MatchSearchTokensToSearchTerms(const TArray<FString> InTokens, ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase);

	/** This overload creates tokens from a string first, then calls ExecuteSearchOnChildNodes(const TArray<FString>& Tokens). */
	void ExecuteSearchOnChildNodes(const FString& SearchString) const;
	void ExecuteSearchOnChildNodes(const TArray<FString>& Tokens) const;

	[[nodiscard]] bool GetDoesRowPassFilters() const;
	void SetDoesRowPassFilters(const bool bPass);

	[[nodiscard]] bool GetIsSelected();

	[[nodiscard]] bool ShouldRowWidgetBeVisible() const;
	[[nodiscard]] EVisibility GetDesiredRowWidgetVisibility() const;

	[[nodiscard]] bool HasVisibleChildRowWidgets() const;

	[[nodiscard]] FText GetDisplayName(const bool bIsHybridRow = false) const;

	[[nodiscard]] const FText& GetDisplayNameOverride() const
	{
		return DisplayNameOverride;
	}

	void SetDisplayNameOverride(const FText& InDisplayNameOverride)
	{
		DisplayNameOverride = InDisplayNameOverride;
	}

	[[nodiscard]] TWeakPtr<SObjectMixerEditorList> GetListViewPtr() const
	{
		return ListViewPtr;
	}

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode();

	[[nodiscard]] TArray<FObjectMixerEditorListRowPtr> GetSelectedTreeViewItems() const;

	const FSlateBrush* GetObjectIconBrush();

	bool GetObjectVisibility();
	void SetObjectVisibility(const bool bNewIsVisible, const bool bIsRecursive = false);

	bool IsThisRowSolo();
	void SetRowSoloState(const bool bNewSolo);

	void ClearSoloRows() const;
	
	TMap<FName, TWeakPtr<IPropertyHandle>> PropertyNamesToHandles;

private:

	FObjectMixerEditorListRowPtr GetAsShared();

	TWeakObjectPtr<UObject> ObjectRef;
	FName FolderPath = NAME_None;
	EObjectMixerEditorListRowType RowType = MatchingObject;
	TArray<FObjectMixerEditorListRowPtr> ChildRows;
	
	TWeakPtr<SObjectMixerEditorList> ListViewPtr;

	FText DisplayNameOverride;

	int32 SortOrder = -1;

	FString CachedSearchTerms;

	bool bDoesRowMatchSearchTerms = true;
	bool bDoesRowPassFilters = true;
	
	bool bIsSelected = false;
	TWeakPtr<FObjectMixerEditorListRow> DirectParentRow;

	// Used to expand all children on shift+click.
	bool bShouldExpandAllChildren = false;

	int32 CachedHybridRowIndex = INDEX_NONE;
};
