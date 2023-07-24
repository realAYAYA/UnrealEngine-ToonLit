// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"

#include "CommonGenericInputActionDataTable.generated.h"

class UCommonGenericInputActionDataTable;

/**
 * Overrides postload to allow for derived classes to perform code-level changes to the datatable.
 * Ex: Per-platform edits. Allows modification of datatable data without checking out the data table asset.
 */
UCLASS(BlueprintType)
class COMMONUI_API UCommonGenericInputActionDataTable : public UDataTable
{
	GENERATED_BODY()

public:
	UCommonGenericInputActionDataTable();
	virtual ~UCommonGenericInputActionDataTable() = default;

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject Interface
};

/**
 * Derive from to process common input action datatable
 */
UCLASS(Transient)
class COMMONUI_API UCommonInputActionDataProcessor : public UObject
{
	GENERATED_BODY()

public:
	UCommonInputActionDataProcessor() = default;
	virtual ~UCommonInputActionDataProcessor() = default;

	virtual void ProcessInputActions(UCommonGenericInputActionDataTable* InputActionDataTable);
};