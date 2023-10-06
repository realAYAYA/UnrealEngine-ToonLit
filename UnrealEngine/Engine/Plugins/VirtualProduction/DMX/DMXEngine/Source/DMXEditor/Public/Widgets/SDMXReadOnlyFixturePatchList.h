// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Library/DMXEntityReference.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"

#include "SDMXReadOnlyFixturePatchList.generated.h"

struct FDMXEntityFixturePatchRef;
class SDMXReadOnlyFixturePatchListRow;
class UDMXEntity;
class UDMXLibrary;

class ITableRow;
class SHeaderRow;
template <typename ItemType> class SListView;
class SSearchBox;
class STableViewBase;


enum class EDMXReadOnlyFixturePatchListShowMode : uint8
{
	/** Show All Fixture Patches in the list */
	All,

	/** Show Only Active Fixture Patches in the list */
	Active,

	/** Show Only Inactive Fixture Patches in the list */
	Inactive
};

/** Collumn IDs in the Fixture Patch List */
struct DMXEDITOR_API FDMXReadOnlyFixturePatchListCollumnIDs
{
	static const FName EditorColor;
	static const FName FixturePatchName;
	static const FName FixtureID;
	static const FName FixtureType;
	static const FName Mode;
	static const FName Patch;
};

/** Struct to describe a fixture patch list, so it can be stored in settings */
USTRUCT()
struct DMXEDITOR_API FDMXReadOnlyFixturePatchListDescriptor
{
	GENERATED_BODY()

	FDMXReadOnlyFixturePatchListDescriptor()
		: SortedByColumnID(FDMXReadOnlyFixturePatchListCollumnIDs::Patch)
		, ColumnIDToShowStateMap(TMap<FName, bool>())
	{
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor) = true;
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName) = true;
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID) = true;
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType) = false;
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::Mode) = false;
		ColumnIDToShowStateMap.Add(FDMXReadOnlyFixturePatchListCollumnIDs::Patch) = true;
	}

	/** By which column ID the List is sorted */
	UPROPERTY()
	FName SortedByColumnID;

	/** Map for storing the show state of each column by name ID */
	UPROPERTY()
	TMap<FName, bool> ColumnIDToShowStateMap;
};

/** Generic list of Fixture Patches in a DMX library for read only purposes */
class DMXEDITOR_API SDMXReadOnlyFixturePatchList
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FDMXFixturePatchListRowDelegate, const TSharedPtr<FDMXEntityFixturePatchRef>)
	DECLARE_DELEGATE_RetVal_OneParam(bool, FDMXFixturePatchListRowRetValDelegate, const TSharedPtr<FDMXEntityFixturePatchRef>)
	DECLARE_DELEGATE_TwoParams(FDMXFixturePatchListRowSelectionDelegate, const TSharedPtr<FDMXEntityFixturePatchRef>, ESelectInfo::Type)

	SLATE_BEGIN_ARGS(SDMXReadOnlyFixturePatchList)
	: _ListDescriptor(FDMXReadOnlyFixturePatchListDescriptor())
	, _DMXLibrary(nullptr)
	{}

		SLATE_ARGUMENT(FDMXReadOnlyFixturePatchListDescriptor, ListDescriptor)

		/** The displayed DMX Library */
		SLATE_ARGUMENT(UDMXLibrary*, DMXLibrary)

		/** Fixture patches not displayed in the list */
		SLATE_ARGUMENT(TArray<FDMXEntityFixturePatchRef>, ExcludedFixturePatches)

		/** Called when a row of the list is right clicked */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		
		/** Called when row selection has changed */
		SLATE_EVENT(FDMXFixturePatchListRowSelectionDelegate, OnRowSelectionChanged)

		/** Called when a row is clicked */
		SLATE_EVENT(FDMXFixturePatchListRowDelegate, OnRowClicked)

		/** Called when a row is double clicked */
		SLATE_EVENT(FDMXFixturePatchListRowDelegate, OnRowDoubleClicked)

		/** Called to get the enable state of each row of the list */
		SLATE_EVENT(FDMXFixturePatchListRowRetValDelegate, IsRowEnabled)

		/** Called to get the visibility state of each row of the list */
		SLATE_EVENT(FDMXFixturePatchListRowRetValDelegate, IsRowVisibile)

		/** Delegate executed when a row was dragged */
		SLATE_EVENT(FOnDragDetected, OnRowDragDetected)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Updates the List on the next tick */
	void RequestListRefresh();

	/** Gets current selected Fixture Patches from the list */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> GetSelectedFixturePatchRefs() const;

	/** Gets all visible Fixture Patches from the list */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> GetVisibleFixturePatchRefs() const;

	/** Gets the current Show Mode of the list */
	EDMXReadOnlyFixturePatchListShowMode GetShowMode() const { return ShowMode; }

	/** Gets the a descriptor for the current parameters for this list */
	FDMXReadOnlyFixturePatchListDescriptor MakeListDescriptor() const;

	/** Gets the current displayed DMX library */
	UDMXLibrary* GetDMXLibrary() const { return WeakDMXLibrary.Get(); }

	/** Sets the displayed DMX library */
	void SetDMXLibrary(UDMXLibrary* InDMXLibrary);

	/** Sets the selection state of the given item, if valid */
	void SetItemSelection(const TSharedPtr<FDMXEntityFixturePatchRef> SelectedItem, bool bSelected);

	/** Sets the excluded fixture patches */
	void SetExcludedFixturePatches(const TArray<FDMXEntityFixturePatchRef>& NewExcludedFixturePatches);

	/** Returns the items displayed int his list */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> GetListItems() const { return ListItems; }

	/** Selects specified items in the list */
	void SelectItems(const TArray<TSharedPtr<FDMXEntityFixturePatchRef>>& ItemsToSelect, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

protected:
	/** Initializes the list parameters using the given ListDescriptor */
	virtual void InitializeByListDescriptor(const FDMXReadOnlyFixturePatchListDescriptor& InListDescriptor);

	/** Refreshes the Fixture Patch List */
	virtual void RefreshList();

	/** Updates ListItems array */
	void UpdateListItems();

	/** Filters ListItems array by the given search text */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> FilterListItems(const FText& SearchText);

	/** Called when a row in the List gets generated */
	virtual TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDMXEntityFixturePatchRef> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called to get wheter a row of the list is enabled or not */
	virtual bool IsRowEnabled(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** Called to get wheter a row of the list is visible or not */
	virtual EVisibility GetRowVisibility(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** Called when text from searchbox is changed */
	void OnSearchTextChanged(const FText& SearchText);

	/** Called when entities were added or removed from the DMX Library */
	void OnEntityAddedOrRemoved(UDMXLibrary* InDMXLibrary, TArray<UDMXEntity*> Entities);

	/** Called when a Fixture Patch changed */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch);

	/** Called when a Fixture Type changed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType);

	/** Generates the Header Row of the List */
	virtual TSharedRef<SHeaderRow> GenerateHeaderRow();

	/** Generates a visibility menu fot the Header Row of the List */
	TSharedRef<SWidget> GenerateHeaderRowVisibilityMenu();

	/** Toggles show state of the column with the given ID */
	void ToggleColumnShowState(const FName ColumnID);

	/** Gets show state of the column with the given ID */
	bool IsColumnShown(const FName ColumnID) const;

	/** Returns the column sort mode for the list*/
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnID) const;

	/** Sort the list by ColumnId */
	void SortByColumnID(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	/** Sets the current ShowMode */
	void SetShowMode(EDMXReadOnlyFixturePatchListShowMode NewShowMode);

	/** Returns true if the ShowModeToCheck is the one this widget uses */
	bool IsUsingShowMode(EDMXReadOnlyFixturePatchListShowMode ShowModeToCheck) const;

	/** The current Sort Mode */
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;

	/** The current Show Mode */
	EDMXReadOnlyFixturePatchListShowMode ShowMode;

	/** By which column ID the List is sorted */
	FName SortedByColumnID;

	/** Map for storing the show state of each column by name ID */
	TMap<FName, bool> ColumnIDToShowStateMap;

	/** Timer handle for the Request List Refresh method */
	FTimerHandle ListRefreshTimerHandle;

	/** The Header Row of the List */
	TSharedPtr<SSearchBox> SearchBox;

	/** The Header Row of the List */
	TSharedPtr<SHeaderRow> HeaderRow;

	/** The list of Fixture Patch references */
	TSharedPtr<SListView<TSharedPtr<FDMXEntityFixturePatchRef>>> ListView;

	/** Rows of Mode widgets in the List */
	TArray<TSharedPtr<SDMXReadOnlyFixturePatchListRow>> ListRows;

	/** Array of current Fixture Patch Ref list items */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> ListItems;

	/** Current displayed DMX Library */
	TWeakObjectPtr<UDMXLibrary> WeakDMXLibrary;

	// Slate Arguments
	TArray<FDMXEntityFixturePatchRef> ExcludedFixturePatches;
	FDMXFixturePatchListRowRetValDelegate IsRowEnabledDelegate;
	FDMXFixturePatchListRowRetValDelegate IsRowVisibleDelegate;
	FOnDragDetected OnRowDragDetectedDelegate;
};
