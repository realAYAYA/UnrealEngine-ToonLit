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

	// FallbackValue will be used as the output value if the all rows in the chooser fail, and the FallbackResult from the chooser is used.
	UPROPERTY(EditAnywhere, Category = "Data")
	FChooserOutputEnumRowData FallbackValue;

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

	virtual void AddToDetails (FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
	virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
#endif
	
	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterEnumBase);
};