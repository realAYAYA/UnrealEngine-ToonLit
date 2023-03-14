// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet2/ListenerManager.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UIAction.h"
#include "AssetRegistry/AssetData.h"

struct FDataTableEditorColumnHeaderData
{
	/** Unique ID used to identify this column */
	FName ColumnId;

	/** Display name of this column */
	FText DisplayName;

	/** The calculated width of this column taking into account the cell data for each row */
	float DesiredColumnWidth;

	/** The FProperty for the variable in this column */
	const FProperty* Property;
};

struct FDataTableEditorRowListViewData
{
	/** Unique ID used to identify this row */
	FName RowId;

	/** Display name of this row */
	FText DisplayName;

	/** The calculated height of this row taking into account the cell data for each column */
	float DesiredRowHeight;

	/** Insertion number of the row */
	uint32 RowNum;

	/** Array corresponding to each cell in this row */
	TArray<FText> CellData;
};

typedef TSharedPtr<FDataTableEditorColumnHeaderData> FDataTableEditorColumnHeaderDataPtr;
typedef TSharedPtr<FDataTableEditorRowListViewData>  FDataTableEditorRowListViewDataPtr;


enum class ERowInsertionPosition
{
	Above,
	Below,
	Bottom,
};

struct UNREALED_API FDataTableEditorUtils
{
	enum class EDataTableChangeInfo
	{
		/** The data corresponding to a single row has been changed */
		RowData,
		/** The data corresponding to the entire list of rows has been changed */
		RowList,
	};

	enum class ERowMoveDirection
	{
		Up,
		Down,
	};

	class FDataTableEditorManager : public FListenerManager < UDataTable, EDataTableChangeInfo >
	{
		FDataTableEditorManager() {}
	public:
		UNREALED_API static FDataTableEditorManager& Get();

		class UNREALED_API ListenerType : public InnerListenerType<FDataTableEditorManager>
		{
		public:
			virtual void SelectionChange(const UDataTable* DataTable, FName RowName) { }
		};
	};

	typedef FDataTableEditorManager::ListenerType INotifyOnDataTableChanged;

	static bool RemoveRow(UDataTable* DataTable, FName Name);
	static uint8* AddRow(UDataTable* DataTable, FName RowName);
	static uint8* DuplicateRow(UDataTable* DataTable, FName SourceRowName, FName RowName);
	static bool RenameRow(UDataTable* DataTable, FName OldName, FName NewName);
	static bool MoveRow(UDataTable* DataTable, FName RowName, ERowMoveDirection Direction, int32 NumRowsToMoveBy = 1);
	static bool SelectRow(const UDataTable* DataTable, FName RowName);
	static bool DiffersFromDefault(UDataTable* DataTable, FName RowName);
	static bool ResetToDefault(UDataTable* DataTable, FName RowName);

	static uint8* AddRowAboveOrBelowSelection(UDataTable* DataTable, const FName& RowName, const FName& NewRowName, ERowInsertionPosition InsertPosition);

	static void BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static void BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info);

	/** Reads a data table and parses out editable copies of rows and columns */
	static void CacheDataTableForEditing(const UDataTable* DataTable, TArray<FDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	/** Generic version that works with any datatable-like structure */
	static void CacheDataForEditing(const UScriptStruct* RowStruct, const TMap<FName, uint8*>& RowMap, TArray<FDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	/** Returns all script structs that can be used as a data table row. This only includes loaded ones */
	static TArray<UScriptStruct*> GetPossibleStructs();

	/** Fills in an array with all possible DataTable structs, unloaded and loaded */
	static void GetPossibleStructAssetData(TArray<FAssetData>& StructAssets);
	
	/** Utility function which verifies that the specified struct type is viable for data tables */
	static bool IsValidTableStruct(const UScriptStruct* Struct);

	/** Add a UI action for search for references, useful for customizations */
	static void AddSearchForReferencesContextMenu(class FDetailWidgetRow& RowNameDetailWidget, FExecuteAction SearchForReferencesAction);

	/** Short description for a data or curve handle */
	static FText GetHandleShortDescription(const UObject* TableAsset, FName RowName);

	/** Tooltip text for the data table row type */
	static FText GetRowTypeInfoTooltipText(FDataTableEditorColumnHeaderDataPtr ColumnHeaderDataPtr);

	/** Doc excerpt name for the data table row type */
	static FString GetRowTypeTooltipDocExcerptName(FDataTableEditorColumnHeaderDataPtr ColumnHeaderDataPtr);

	/** Link to variable type doc  */
	static const FString VariableTypesTooltipDocLink;

	/** Delegate called when a data table struct is selected */
	DECLARE_DELEGATE_OneParam(FOnDataTableStructSelected, UScriptStruct*);

	/** Creates a combo box that allows selecting from the list of possible row structures */
	static TSharedRef<SWidget> MakeRowStructureComboBox(FOnDataTableStructSelected OnSelected);
};
