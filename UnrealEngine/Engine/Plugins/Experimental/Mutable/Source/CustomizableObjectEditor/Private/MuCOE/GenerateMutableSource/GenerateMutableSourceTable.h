// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Table.h"

class FProperty;
class FString;

class UCustomizableObjectNodeTable;
class UEdGraphPin;
struct FMutableGraphGenerationContext;


/** 
 *
 * @param TableNode 
 * @param Pin 
 * @param MutableTable 
 * @param DataTableColumnName 
 * @param TableProperty 
 * @param LODIndexConnected LOD which the pin is connected to.
 * @param SectionIndexConnected Section which the pin is connected to.
 * @param LODIndex LOD we are generating. Will be different from LODIndexConnected only when using Automatic LOD From Mesh. 
 * @param SectionIndex Section we are generating. Will be different from SectionIndexConnected only when using Automatic LOD From Mesh.
 * @param bOnlyConnectedLOD Corrected LOD and Section will unconditionally always be the connected ones.
 * @param GenerationContext 
 * @return  */
bool GenerateTableColumn(const UCustomizableObjectNodeTable* TableNode, const UEdGraphPin* Pin, mu::TablePtr MutableTable, const FString& DataTableColumnName, const FProperty* ColumnProperty,
	int32 LODIndexConnected, int32 SectionIndexConnected,
	int32 LODIndex, int32 SectionIndex,
	bool bOnlyConnectedLOD,
	FMutableGraphGenerationContext& GenerationContext);


/**
 *
 * @param TableNode 
 * @param MutableTable 
 * @param ColumnName 
 * @param TableProperty 
 * @param RowName 
 * @param RowIdx 
 * @param CellData 
 * @param Property 
 * @param LODIndexConnected LOD which the pin is connected to.
 * @param SectionIndexConnected Section which the pin is connected to.
 * @param LODIndex LOD we are generating. Will be different from LODIndexConnected only when using Automatic LOD From Mesh. 
 * @param SectionIndex Section we are generating. Will be different from SectionIndexConnected only when using Automatic LOD From Mesh.
 * @param bOnlyConnectedLOD Corrected LOD and Section will unconditionally always be the connected ones.
 * @param GenerationContext 
 * @return  */
bool FillTableColumn(const UCustomizableObjectNodeTable* TableNode, mu::TablePtr MutableTable, const FString& ColumnName,
	const FString& RowName, int32 RowIdx, uint8* CellData, const FProperty* ColumnProperty,
	int32 LODIndexConnected, int32 SectionIndexConnected, int32 LODIndex, int32 SectionIndex,
	bool bOnlyConnectedLOD,
	FMutableGraphGenerationContext& GenerationContext);


mu::TablePtr GenerateMutableSourceTable(const FString& TableName, const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);
