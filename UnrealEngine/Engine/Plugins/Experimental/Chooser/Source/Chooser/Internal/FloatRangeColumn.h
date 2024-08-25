// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <IHasContext.h>

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterFloat.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "Serialization/MemoryReader.h"
#include "FloatRangeColumn.generated.h"

USTRUCT(DisplayName = "Float Property Binding")
struct CHOOSER_API FFloatContextProperty :  public FChooserParameterFloatBase
{
	GENERATED_BODY()
	
	virtual bool GetValue(FChooserEvaluationContext& Context, double& OutResult) const override;
	virtual bool SetValue(FChooserEvaluationContext& Context, double Value) const override;

	UPROPERTY()
	TArray<FName> PropertyBindingChain_DEPRECATED;
	
	UPROPERTY(EditAnywhere, Meta = (BindingType = "double", BindingAllowFunctions = "true", BindingColor = "FloatPinTypeColor"), Category = "Binding")
	FChooserPropertyBinding Binding;

	virtual void PostLoad() override
	{
		if (PropertyBindingChain_DEPRECATED.Num() > 0)
		{
			Binding.PropertyBindingChain = PropertyBindingChain_DEPRECATED;
			PropertyBindingChain_DEPRECATED.SetNum(0);
		}
	}
	
	CHOOSER_PARAMETER_BOILERPLATE();
};

USTRUCT()
struct FChooserFloatRangeRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Runtime)
	float Min=0;
	
	UPROPERTY(EditAnywhere, Category=Runtime)
	float Max=0;
};


USTRUCT()
struct CHOOSER_API FFloatRangeColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FFloatRangeColumn();
		
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterFloatBase"), Category = "Data")
	FInstancedStruct InputValue;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category="Data")
	FChooserFloatRangeRowData DefaultRowValue;
#endif
	
	UPROPERTY(EditAnywhere, Category="Data")
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<FChooserFloatRangeRowData> RowValues;
	
	virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;

#if WITH_EDITOR
	mutable double TestValue = 0.0;
	virtual bool EditorTestFilter(int32 RowIndex) const override
	{
		return RowValues.IsValidIndex(RowIndex) && TestValue >= RowValues[RowIndex].Min && TestValue <= RowValues[RowIndex].Max;
	}
	
	virtual void SetTestValue(TArrayView<const uint8> Value) override
	{
		FMemoryReaderView Reader(Value);
		Reader << TestValue;
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

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterFloatBase);

};

// deprecated class version to support upgrading old data

UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserParameterFloat_ContextProperty :  public UObject, public IChooserParameterFloat
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FFloatContextProperty::StaticStruct());
		FFloatContextProperty& Property = OutInstancedStruct.GetMutable<FFloatContextProperty>();
		Property.Binding.PropertyBindingChain = PropertyBindingChain;
	}
};

UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserColumnFloatRange : public UObject, public IChooserColumn
{
	GENERATED_BODY()
	public:
	UDEPRECATED_ChooserColumnFloatRange() {}
	UDEPRECATED_ChooserColumnFloatRange(const FObjectInitializer& ObjectInitializer)
	{
		InputValue = ObjectInitializer.CreateDefaultSubobject<UDEPRECATED_ChooserParameterFloat_ContextProperty>(this, "InputValue");
	}	
		
	UPROPERTY(EditAnywhere, Category = "Input")
	TScriptInterface<IChooserParameterFloat> InputValue;
	
	UPROPERTY(EditAnywhere, Category=Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<FChooserFloatRangeRowData> RowValues;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FFloatRangeColumn::StaticStruct());
		FFloatRangeColumn& Column = OutInstancedStruct.GetMutable<FFloatRangeColumn>();
		if (IChooserParameterFloat* InputValueInterface = InputValue.GetInterface())
		{
			InputValueInterface->ConvertToInstancedStruct(Column.InputValue);
		}
		Column.RowValues = RowValues;
	}
};