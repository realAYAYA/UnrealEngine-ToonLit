// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "UObject/Class.h" // for FStructUtils
#include "DataTableFunctionLibrary.generated.h"

class UCurveTable;

/** Enum used to indicate success or failure of EvaluateCurveTableRow. */
UENUM()
namespace EEvaluateCurveTableResult
{
    enum Type
    {
        /** Found the row successfully. */
        RowFound,
        /** Failed to find the row. */
        RowNotFound,
    };
}

UCLASS()
class ENGINE_API UDataTableFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "DataTable", meta = (ExpandEnumAsExecs="OutResult", DataTablePin="CurveTable"))
	static void EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY,const FString& ContextString);
    
	// Returns whether or not Table contains a row named RowName
  	UFUNCTION(BlueprintCallable, Category = "DataTable")
 	static bool DoesDataTableRowExist(UDataTable* Table, FName RowName);
    
	UFUNCTION(BlueprintCallable, Category = "DataTable")
	static void GetDataTableRowNames(UDataTable* Table, TArray<FName>& OutRowNames);

	/** Export from the DataTable all the row for one column. Export it as string. The row name is not included. */
	UFUNCTION(BlueprintCallable, Category = "DataTable")
	static TArray<FString> GetDataTableColumnAsString(const UDataTable* DataTable, FName PropertyName);

    /** Get a Row from a DataTable given a RowName */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "DataTable", meta=(CustomStructureParam = "OutRow", BlueprintInternalUseOnly="true"))
    static bool GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow);
    
	static bool Generic_GetDataTableRowFromName(const UDataTable* Table, FName RowName, void* OutRowPtr);

    /** Based on UDataTableFunctionLibrary::GetDataTableRow */
    DECLARE_FUNCTION(execGetDataTableRowFromName)
    {
        P_GET_OBJECT(UDataTable, Table);
        P_GET_PROPERTY(FNameProperty, RowName);
        
        Stack.StepCompiledIn<FStructProperty>(NULL);
        void* OutRowPtr = Stack.MostRecentPropertyAddress;

		P_FINISH;
		bool bSuccess = false;
		
		FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
		if (!Table)
		{
			FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				NSLOCTEXT("GetDataTableRow", "MissingTableInput", "Failed to resolve the table input. Be sure the DataTable is valid.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		}
		else if(StructProp && OutRowPtr)
		{
			UScriptStruct* OutputType = StructProp->Struct;
			const UScriptStruct* TableType  = Table->GetRowStruct();
		
			const bool bCompatible = (OutputType == TableType) || 
				(OutputType->IsChildOf(TableType) && FStructUtils::TheSameLayout(OutputType, TableType));
			if (bCompatible)
			{
				P_NATIVE_BEGIN;
				bSuccess = Generic_GetDataTableRowFromName(Table, RowName, OutRowPtr);
				P_NATIVE_END;
			}
			else
			{
				FBlueprintExceptionInfo ExceptionInfo(
					EBlueprintExceptionType::AccessViolation,
					NSLOCTEXT("GetDataTableRow", "IncompatibleProperty", "Incompatible output parameter; the data table's type is not the same as the return type.")
					);
				FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
			}
		}
		else
		{
			FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				NSLOCTEXT("GetDataTableRow", "MissingOutputProperty", "Failed to resolve the output parameter for GetDataTableRow.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		}
		*(bool*)RESULT_PARAM = bSuccess;
    }

#if WITH_EDITOR
	/** 
	 * Empty and fill a Data Table from CSV string.
	 * @param	CSVString	The Data that representing the contents of a CSV file.
	 * @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | DataTable", DisplayName="Fill Data Table from CSV String")
	static bool FillDataTableFromCSVString(UDataTable* DataTable, const FString& CSVString);

	/** 
	 * Empty and fill a Data Table from CSV file.
	 * @param	CSVFilePath	The file path of the CSV file.
	 * @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | DataTable", DisplayName = "Fill Data Table from CSV File")
	static bool FillDataTableFromCSVFile(UDataTable* DataTable, const FString& CSVFilePath);

	/** 
	 * Empty and fill a Data Table from JSON string.
	 * @param	JSONString	The Data that representing the contents of a JSON file.
	 * @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | DataTable", DisplayName = "Fill Data Table from JSON String")
	static bool FillDataTableFromJSONString(UDataTable* DataTable, const FString& JSONString);


	/** 
	 * Empty and fill a Data Table from JSON file.
	 * @param	JSONFilePath	The file path of the JSON file.
	 * @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | DataTable", DisplayName = "Fill Data Table from JSON File")
	static bool FillDataTableFromJSONFile(UDataTable* DataTable, const FString& JSONFilePath, UScriptStruct* ImportRowStruct = nullptr);
#endif //WITH_EDITOR
};
