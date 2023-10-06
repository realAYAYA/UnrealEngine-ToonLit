// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowSelection.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "GeometryCollection/ManagedArrayCollection.h"

/**
* Struct to hold OutputType/Selection data for the outputs
* Data is stored in a map using the OutputName as key: TMap<FString, SCollectionInfo>
*/
struct SCollectionInfo
{
//	FString OutputType;
	const FManagedArrayCollection Collection;
};


/** 
* Header
* 1st column possible values: Transform Index/Face Index/Vertex Index based on the OutputType
* 2nd column is SelectionStatus
*/
struct FCollectionSpreadSheetHeader
{
	static const FName IndexColumnName;

	TArray<FName> ColumnNames;
};


/** 
* Representing a row in the table 
* Index/SelectionStatus
*/
struct FCollectionSpreadSheetItem
{
	TArray<FString> Values;
};


/** 
* 
*/
class SCollectionSpreadSheetRow : public SMultiColumnTableRow<TSharedPtr<const FCollectionSpreadSheetItem>>
{
public:
	SLATE_BEGIN_ARGS(SCollectionSpreadSheetRow) 
	{}
		SLATE_ARGUMENT(TSharedPtr<const FCollectionSpreadSheetHeader>, Header)
		SLATE_ARGUMENT(TSharedPtr<const FCollectionSpreadSheetItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, const TSharedPtr<const FCollectionSpreadSheetHeader>& InHeader, const TSharedPtr<const FCollectionSpreadSheetItem>& InItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<const FCollectionSpreadSheetHeader> Header;
	TSharedPtr<const FCollectionSpreadSheetItem> Item;
};

/** 
* 2xn grid to display Collection data
*/
class SCollectionSpreadSheet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCollectionSpreadSheet) {}
		SLATE_ARGUMENT(FName, SelectedOutput)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TMap<FString, SCollectionInfo>& GetCollectionInfoMap() { return CollectionInfoMap; }

	const FName& GetSelectedOutput() const;
	void SetSelectedOutput(const FName& InSelectedOutput);
	
	const FName& GetSelectedGroup() const;
	void SetSelectedGroup(const FName& InSelectedGroup);

	int32 GetNumItems() const { return NumItems; }
	void SetNumItems(int32 InNumItems) { NumItems = InNumItems; }

	TSharedRef<ITableRow> GenerateRow(TSharedPtr<const FCollectionSpreadSheetItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<SScrollBar> SpreadSheetVerticalScrollBar;

	TMap<FString, int32> AttrTypeWidthMap;

private:
	FName SelectedOutput = FName(TEXT(""));
	FName SelectedGroup = FName(TEXT(""));
	TMap<FString, SCollectionInfo> CollectionInfoMap;

	TSharedPtr<SListView<TSharedPtr<const FCollectionSpreadSheetItem>>> ListView;
//	TSharedPtr<STreeView<TSharedPtr<const FCollectionSpreadSheetItem>>> ListView;
	TArray<TSharedPtr<const FCollectionSpreadSheetItem>> ListItems;

	TSharedPtr<FCollectionSpreadSheetHeader> Header;
	TSharedPtr<SHeaderRow> HeaderRowWidget;

	int32 NumItems = 0;
	void RegenerateHeader();
	void RepopulateListView();
};

/**
* Widget for the CollectionSpreadSheet panel
*/
class SCollectionSpreadSheetWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCollectionSpreadSheetWidget) {}
	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct(const FArguments& InArgs);

	void SetData(const FString& InNodeName);
	void RefreshWidget();
	TSharedPtr<SCollectionSpreadSheet> GetCollectionTable() { return CollectionTable; }
	void SetStatusText();

	void UpdateCollectionGroups(const FName& InOutputName);

	/** Gets a multicast delegate which is called whenever the PinnedDown button clicked */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPinnedDownChanged, const bool);
	FOnPinnedDownChanged& GetOnPinnedDownChangedDelegate() { return OnPinnedDownChangedDelegate; }

	/** Gets a multicast delegate which is called whenever the LockRefresh button clicked */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRefreshLockedChanged, const bool);
	FOnRefreshLockedChanged& GetOnRefreshLockedChangedDelegate() { return OnRefreshLockedChangedDelegate; }

	// UI callbacks
	void NodeOutputsComboBoxSelectionChanged(FName InSelectedOutput, ESelectInfo::Type InSelectInfo);
	void CollectionGroupsComboBoxSelectionChanged(FName InSelectedOutput, ESelectInfo::Type InSelectInfo);
	FText GetNoOutputText();
	FText GetNoGroupText();

private:
	TSharedPtr<STextBlock> NodeNameTextBlock;
	TSharedPtr<SComboBox<FName>> NodeOutputsComboBox;
	TSharedPtr<STextBlock> NodeOutputsComboBoxLabel;
	TSharedPtr<SComboBox<FName>> CollectionGroupsComboBox;
	TSharedPtr<STextBlock> CollectionGroupsComboBoxLabel;
	TSharedPtr<SCollectionSpreadSheet> CollectionTable;
	TSharedPtr<STextBlock> StatusTextBlock;

	FString NodeName;
	TArray<FName> NodeOutputs;
	TArray<FName> CollectionGroups;
	bool bIsPinnedDown = false;
	bool bIsRefreshLocked = false;

protected:
	FOnPinnedDownChanged OnPinnedDownChangedDelegate;
	FOnRefreshLockedChanged OnRefreshLockedChangedDelegate;

};