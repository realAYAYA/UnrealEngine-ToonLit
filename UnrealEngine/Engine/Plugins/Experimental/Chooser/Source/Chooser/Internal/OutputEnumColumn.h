// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterEnum.h"
#include "InstancedStruct.h"
#include "ChooserPropertyAccess.h"
#include "OutputEnumColumn.generated.h"

USTRUCT()
struct FChooserOutputEnumRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Runtime)
	uint8 Value = 0;
};


USTRUCT()
struct CHOOSER_API FOutputEnumColumn : public FChooserColumnBase
{
	GENERATED_BODY()
public:
	FOutputEnumColumn();
	virtual bool HasFilters() const override { return false; }
	virtual bool HasOutputs() const override { return true; }
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;

	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterEnumBase"), Category = "Data")
	FInstancedStruct InputValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Data")
	FChooserOutputEnumRowData DefaultRowValue;
#endif
	
	UPROPERTY(EditAnywhere, Category = "Data")
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array
	TArray<FChooserOutputEnumRowData> RowValues;
	
#if WITH_EDITOR
	mutable uint8 TestValue;
#endif
	
	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterEnumBase);
};