// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterRandomize.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "RandomizeColumn.generated.h"

USTRUCT(DisplayName = "Randomize Property Binding")
struct CHOOSER_API FRandomizeContextProperty :  public FChooserParameterRandomizeBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Meta = (BindingType = "FChooserRandomizationContext", BindingAllowFunctions = "false", BindingColor = "StructPinTypeColor"), Category = "Binding")
	FChooserPropertyBinding Binding;
	
	virtual bool GetValue(FChooserEvaluationContext& Context, const FChooserRandomizationContext*& OutResult) const override;

	CHOOSER_PARAMETER_BOILERPLATE();
};

USTRUCT()
struct CHOOSER_API FRandomizeColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FRandomizeColumn();
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.RandomizeContextProperty"), Category = "Data")
	FInstancedStruct InputValue;
	
	
	UPROPERTY(EditAnywhere, Category= "Data", meta=(ClampMin="0.0",Tooltip="Multiplies the weight of the previous chosen result (set to 0 to never pick the same result twice in a row)"));
	float RepeatProbabilityMultiplier = 1.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category= "Data", DisplayName="DefaultRowValue");
	float DefaultRowValue = 1.0f;
#endif
	
	UPROPERTY(EditAnywhere, Category= "Data", DisplayName="RowValues");
	TArray<float> RowValues; 
	
	virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;
	
	virtual bool HasFilters() const override { return true; }
	virtual bool HasOutputs() const override { return true; }

	#if WITH_EDITOR
    	virtual bool EditorTestFilter(int32 RowIndex) const override
    	{
    		return true;
    	}

		virtual void AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
		virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
    #endif

	virtual void PostLoad() override
	{
		if (InputValue.IsValid())
		{
			InputValue.GetMutable<FChooserParameterBase>().PostLoad();
		}
	}

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterRandomizeBase);

#if WITH_EDITOR
	virtual bool IsRandomizeColumn() const override { return true; }
#endif
};