// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Table.h"

class FProperty;
class FString;

class UCustomizableObjectNodeTable;
class UEdGraphPin;
struct FMutableGraphGenerationContext;

void GenerateTableColumn(const UCustomizableObjectNodeTable* TableNode, const UEdGraphPin* Pin, mu::TablePtr MutableTable, const FString& DataTableColumnName, const int32 LOD, FMutableGraphGenerationContext& GenerationContext);

void FillTableColumn(const UCustomizableObjectNodeTable* TableNode, mu::TablePtr MutableTable, const FString& ColumnName,
	const FString& RowName, const int32 RowIdx, uint8* CellData, FProperty* Property, const int32 LOD, FMutableGraphGenerationContext& GenerationContext);

mu::TablePtr GenerateMutableSourceTable(const FString& TableName, const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);
