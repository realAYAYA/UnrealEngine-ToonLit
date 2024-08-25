// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterBool.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "OutputBoolColumn.generated.h"

USTRUCT()
struct CHOOSER_API FOutputBoolColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FOutputBoolColumn();
	virtual bool HasFilters() const override { return false; }
	virtual bool HasOutputs() const override { return true; }
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterBoolBase"), Category = "Hidden")
	FInstancedStruct InputValue;

#if WITH_EDITOR
	mutable bool TestValue=false;

	virtual void AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
	virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
#endif
	
	// FallbackValue will be used as the output value if the all rows in the chooser fail, and the FallbackResult from the chooser is used.
	UPROPERTY(EditAnywhere, Category=Data);
	bool bFallbackValue = false;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Data);
	bool DefaultRowValue = false;
#endif
	
	UPROPERTY(EditAnywhere, Category=Data);
	TArray<bool> RowValues; 

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterBoolBase);
};