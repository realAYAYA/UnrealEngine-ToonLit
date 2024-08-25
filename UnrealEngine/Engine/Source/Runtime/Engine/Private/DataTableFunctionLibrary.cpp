// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/DataTableFunctionLibrary.h"
#include "Engine/CurveTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataTableFunctionLibrary)

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#include "Factories/CSVImportFactory.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#endif //WITH_EDITOR

UDataTableFunctionLibrary::UDataTableFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDataTableFunctionLibrary::EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY,const FString& ContextString)
{
	FCurveTableRowHandle Handle;
	Handle.CurveTable = CurveTable;
	Handle.RowName = RowName;
	
	bool found = Handle.Eval(InXY, &OutXY,ContextString);
	
	if (found)
	{
	    OutResult = EEvaluateCurveTableResult::RowFound;
	}
	else
	{
	    OutResult = EEvaluateCurveTableResult::RowNotFound;
	}
}

const UScriptStruct* UDataTableFunctionLibrary::GetDataTableRowStruct(const UDataTable* Table)
{
	return Table
		? Table->GetRowStruct()
		: nullptr;
}

bool UDataTableFunctionLibrary::DoesDataTableRowExist(const UDataTable* Table, FName RowName)
{
	if (!Table)
	{
		return false;
	}
	else if (Table->RowStruct == nullptr)
	{
		return false;
	}
	return Table->GetRowMap().Find(RowName) != nullptr;
}

TArray<FString> UDataTableFunctionLibrary::GetDataTableColumnAsString(const UDataTable* DataTable, FName PropertyName)
{
	if (DataTable && PropertyName != NAME_None)
	{
		EDataTableExportFlags ExportFlags = EDataTableExportFlags::None;
		return DataTableUtils::GetColumnDataAsString(DataTable, PropertyName, ExportFlags);
	}
	return TArray<FString>();
}

bool UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(const UDataTable* Table, FName RowName, void* OutRowPtr)
{
	bool bFoundRow = false;

	if (OutRowPtr && Table)
	{
		void* RowPtr = Table->FindRowUnchecked(RowName);

		if (RowPtr != nullptr)
		{
			const UScriptStruct* StructType = Table->GetRowStruct();

			if (StructType != nullptr)
			{
				StructType->CopyScriptStruct(OutRowPtr, RowPtr);
				bFoundRow = true;
			}
		}
	}

	return bFoundRow;
}

bool UDataTableFunctionLibrary::GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow)
{
	// We should never hit this!  stubs to avoid NoExport on the class.
	check(0);
	return false;
}

void UDataTableFunctionLibrary::GetDataTableRowNames(const UDataTable* Table, TArray<FName>& OutRowNames)
{
	if (Table)
	{
		OutRowNames = Table->GetRowNames();
	}
	else
	{
		OutRowNames.Empty();
	}
}

void UDataTableFunctionLibrary::GetDataTableColumnNames(const UDataTable* Table, TArray<FName>& OutColumnNames)
{
	if (Table && Table->GetRowStruct())
	{
		OutColumnNames = DataTableUtils::GetStructPropertyNames(Table->GetRowStruct());
	}
	else
	{
		OutColumnNames.Empty();
	}
}

void UDataTableFunctionLibrary::GetDataTableColumnExportNames(const UDataTable* Table, TArray<FString>& OutExportColumnNames)
{
	OutExportColumnNames.Empty();

	if (Table && Table->GetRowStruct())
	{
		for (TFieldIterator<const FProperty> It(Table->GetRowStruct()); It; ++It)
		{
			OutExportColumnNames.Add(DataTableUtils::GetPropertyExportName(*It));
		}
	}
}

bool UDataTableFunctionLibrary::GetDataTableColumnNameFromExportName(const UDataTable* Table, const FString& ColumnExportName, FName& OutColumnName)
{
	if (Table && Table->GetRowStruct())
	{
		for (TFieldIterator<const FProperty> It(Table->GetRowStruct()); It; ++It)
		{
			const FString PropertyExportName = DataTableUtils::GetPropertyExportName(*It);
			if (PropertyExportName == ColumnExportName)
			{
				OutColumnName = It->GetFName();
				return true;
			}
		}
	}

	return false;
}


#if WITH_EDITOR
bool UDataTableFunctionLibrary::FillDataTableFromCSVString(UDataTable* DataTable, const FString& InString, UScriptStruct* ImportRowStruct)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromCSVString - The DataTable is invalid."));
		return false;
	}

	UCSVImportFactory* ImportFactory = NewObject<UCSVImportFactory>();
	ImportFactory->AutomatedImportSettings.bForceAutomatedImport = ImportRowStruct != nullptr;
	ImportFactory->AutomatedImportSettings.ImportRowStruct = ImportRowStruct;

	bool bWasCancelled = false;
	const TCHAR* Buffer = *InString;
	UObject* Result = ImportFactory->FactoryCreateText(DataTable->GetClass()
		, DataTable->GetOuter()
		, DataTable->GetFName()
		, DataTable->GetFlags()
		, nullptr
		, TEXT("csv")
		, Buffer
		, Buffer + InString.Len()
		, nullptr
		, bWasCancelled);

	return Result != nullptr && !bWasCancelled;
}

bool UDataTableFunctionLibrary::FillDataTableFromCSVFile(UDataTable* DataTable, const FString& InFilePath, UScriptStruct* ImportRowStruct)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromCSVFile - The DataTable is invalid."));
		return false;
	}

	if (!IFileManager::Get().FileExists(*InFilePath))
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromCSVFile - The file '%s' doesn't exist."), *InFilePath);
		return false;
	}

	DataTable->AssetImportData->Update(InFilePath);

	UCSVImportFactory* ImportFactory = NewObject<UCSVImportFactory>();
	ImportFactory->AutomatedImportSettings.bForceAutomatedImport = ImportRowStruct != nullptr;
	ImportFactory->AutomatedImportSettings.ImportRowStruct = ImportRowStruct;
	
	return ImportFactory->ReimportCSV(DataTable) == EReimportResult::Succeeded;
}

bool UDataTableFunctionLibrary::FillDataTableFromJSONString(UDataTable* DataTable, const FString& InString, UScriptStruct* ImportRowStruct)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromJSONString - The DataTable is invalid."));
		return false;
	}

	UCSVImportFactory* ImportFactory = NewObject<UCSVImportFactory>();
	ImportFactory->AutomatedImportSettings.bForceAutomatedImport = ImportRowStruct != nullptr;
	ImportFactory->AutomatedImportSettings.ImportRowStruct = ImportRowStruct;

	bool bWasCancelled = false;
	const TCHAR* Buffer = *InString;
	UObject* Result = ImportFactory->FactoryCreateText(DataTable->GetClass()
		, DataTable->GetOuter()
		, DataTable->GetFName()
		, DataTable->GetFlags()
		, nullptr
		, TEXT("json")
		, Buffer
		, Buffer + InString.Len()
		, nullptr
		, bWasCancelled);

	return Result != nullptr && !bWasCancelled;
}

bool UDataTableFunctionLibrary::FillDataTableFromJSONFile(UDataTable* DataTable, const FString& InFilePath, UScriptStruct* ImportRowStruct)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromJSONFile - The DataTable is invalid."));
		return false;
	}

	if (!IFileManager::Get().FileExists(*InFilePath))
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromJSONFile - The file '%s' doesn't exist."), *InFilePath);
		return false;
	}

	if (!InFilePath.EndsWith(TEXT(".json")))
	{
		UE_LOG(LogDataTable, Error, TEXT("FillDataTableFromJSONFile - The file is not a JSON file."));
		return false;
	}

	DataTable->AssetImportData->Update(InFilePath);

	UCSVImportFactory* ImportFactory = NewObject<UCSVImportFactory>();
	ImportFactory->AutomatedImportSettings.bForceAutomatedImport = ImportRowStruct != nullptr;
	ImportFactory->AutomatedImportSettings.ImportRowStruct = ImportRowStruct;
	
	return ImportFactory->ReimportCSV(DataTable) == EReimportResult::Succeeded;
}

bool UDataTableFunctionLibrary::ExportDataTableToCSVString(const UDataTable* DataTable, FString& OutCSVString)
{
	if (!DataTable || !DataTable->GetRowStruct())
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToCSVString - The DataTable is invalid."));
		return false;
	}

	OutCSVString = DataTable->GetTableAsCSV();
	return true;
}

bool UDataTableFunctionLibrary::ExportDataTableToCSVFile(const UDataTable* DataTable, const FString& CSVFilePath)
{
	if (!DataTable || !DataTable->GetRowStruct())
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToCSVFile - The DataTable is invalid."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(DataTable->GetTableAsCSV(), *CSVFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToCSVFile - Failed to write file '%s'."), *CSVFilePath);
		return false;
	}

	return true;
}

bool UDataTableFunctionLibrary::ExportDataTableToJSONString(const UDataTable* DataTable, FString& OutJSONString)
{
	if (!DataTable || !DataTable->GetRowStruct())
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToJSONString - The DataTable is invalid."));
		return false;
	}

	OutJSONString = DataTable->GetTableAsJSON();
	return true;
}

bool UDataTableFunctionLibrary::ExportDataTableToJSONFile(const UDataTable* DataTable, const FString& JSONFilePath)
{
	if (!DataTable || !DataTable->GetRowStruct())
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToJSONFile - The DataTable is invalid."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(DataTable->GetTableAsJSON(), *JSONFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogDataTable, Error, TEXT("ExportDataTableToJSONFile - Failed to write file '%s'."), *JSONFilePath);
		return false;
	}

	return true;
}

void UDataTableFunctionLibrary::AddDataTableRow(UDataTable* const DataTable, const FName& RowName, const FTableRowBase& RowData)
{
	if (!DataTable)
	{
		UE_LOG(LogDataTable, Error, TEXT("AddDataTableRow - The DataTable is invalid."));
		return;
	}

	DataTable->AddRow(RowName, RowData);
}

DEFINE_FUNCTION(UDataTableFunctionLibrary::execAddDataTableRow)
{
	P_GET_OBJECT(UDataTable, DataTable);
	P_GET_PROPERTY(FNameProperty, RowName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FTableRowBase* const RowData = reinterpret_cast<FTableRowBase*>(Stack.MostRecentPropertyAddress);
	const FStructProperty* const StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	if (!DataTable)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AccessViolation,
			NSLOCTEXT("AddDataTableRow", "MissingTableInput", "Failed to resolve the DataTable parameter.")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else if (StructProp && RowData)
	{
		const UScriptStruct* const RowType = StructProp->Struct;
		const UScriptStruct* const TableType = DataTable->GetRowStruct();

		// If the row type is compatible with the table type ...
		const bool bIsTableRow = RowType->IsChildOf(FTableRowBase::StaticStruct());
		const bool bMatchesTableType = RowType == TableType;
		if (bIsTableRow && (bMatchesTableType || (RowType->IsChildOf(TableType) && FStructUtils::TheSameLayout(RowType, TableType))))
		{
			P_NATIVE_BEGIN;
			AddDataTableRow(DataTable, RowName, *RowData);
			P_NATIVE_END;
		}
		else
		{
			FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				NSLOCTEXT("AddDataTableRow", "IncompatibleProperty", "The data table type is incompatible with the RowData parameter.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		}
	}
	else
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AccessViolation,
			NSLOCTEXT("AddDataTableRow", "MissingOutputProperty", "Failed to resolve the RowData parameter.")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
}
#endif //WITH_EDITOR
