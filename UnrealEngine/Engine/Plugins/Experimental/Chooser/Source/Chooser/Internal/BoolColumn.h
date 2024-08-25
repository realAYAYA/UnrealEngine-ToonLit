// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterBool.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "Serialization/MemoryReader.h"
#include "BoolColumn.generated.h"

UENUM()
enum class EBoolColumnCellValue : uint8
{
	MatchFalse = 0,
	MatchTrue = 1,
	MatchAny = 2,
};


USTRUCT(DisplayName = "Bool Property Binding")
struct CHOOSER_API FBoolContextProperty :  public FChooserParameterBoolBase
{
	GENERATED_BODY()
public:

	UPROPERTY()
	TArray<FName> PropertyBindingChain_DEPRECATED;
	
	UPROPERTY(EditAnywhere, Meta = (BindingType = "bool", BindingAllowFunctions = "true", BindingColor = "BooleanPinTypeColor"), Category = "Binding")
	FChooserPropertyBinding Binding;
	
	virtual bool GetValue(FChooserEvaluationContext& Context, bool& OutResult) const override;
	virtual bool SetValue(FChooserEvaluationContext& Context, bool InValue) const override;

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
struct CHOOSER_API FBoolColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FBoolColumn();
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterBoolBase"), Category = "Data")
	FInstancedStruct InputValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<bool> RowValues_DEPRECATED;

	UPROPERTY(EditAnywhere, Category= "Data", DisplayName="DefaultRowValue");
	EBoolColumnCellValue DefaultRowValue = EBoolColumnCellValue::MatchAny;
#endif
	
	UPROPERTY(EditAnywhere, Category= "Data", DisplayName="RowValues");
	TArray<EBoolColumnCellValue> RowValuesWithAny; 
	
	virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;

#if WITH_EDITOR
	mutable bool TestValue = false;
	virtual bool EditorTestFilter(int32 RowIndex) const override
	{
		return RowValuesWithAny.IsValidIndex(RowIndex) && (RowValuesWithAny[RowIndex] == EBoolColumnCellValue::MatchAny || TestValue == static_cast<bool>(RowValuesWithAny[RowIndex]));
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
#if WITH_EDITORONLY_DATA
		if (RowValues_DEPRECATED.Num() > 0)
		{
			RowValuesWithAny.SetNum(0,EAllowShrinking::No);
			RowValuesWithAny.Reserve(RowValues_DEPRECATED.Num());
			for(bool Value : RowValues_DEPRECATED)
			{
				RowValuesWithAny.Add(Value ? EBoolColumnCellValue::MatchTrue : EBoolColumnCellValue::MatchFalse);
			}
			RowValues_DEPRECATED.SetNum(0);
		}
#endif
		
		if (InputValue.IsValid())
		{
			InputValue.GetMutable<FChooserParameterBase>().PostLoad();
		}
	}

	CHOOSER_COLUMN_BOILERPLATE2(FChooserParameterBoolBase, RowValuesWithAny);
};

// deprecated class versions for converting old data

UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserParameterBool_ContextProperty :  public UObject, public IChooserParameterBool
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FBoolContextProperty::StaticStruct());
		FBoolContextProperty& Property = OutInstancedStruct.GetMutable<FBoolContextProperty>();
		Property.Binding.PropertyBindingChain = PropertyBindingChain;
	}
};

UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserColumnBool : public UObject, public IChooserColumn
{
	GENERATED_BODY()
public:
	UDEPRECATED_ChooserColumnBool() {};
	UDEPRECATED_ChooserColumnBool(const FObjectInitializer& ObjectInitializer)
	{
		InputValue = ObjectInitializer.CreateDefaultSubobject<UDEPRECATED_ChooserParameterBool_ContextProperty>(this, "InputValue");
	}	
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TScriptInterface<IChooserParameterBool> InputValue;

	UPROPERTY(EditAnywhere, Category=Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<bool> RowValues;

	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FBoolColumn::StaticStruct());
		FBoolColumn& Column = OutInstancedStruct.GetMutable<FBoolColumn>();
		if (IChooserParameterBool* InputValueInterface = InputValue.GetInterface())
		{
			InputValueInterface->ConvertToInstancedStruct(Column.InputValue);
		}

		Column.RowValuesWithAny.SetNum(0,EAllowShrinking::No);
		Column.RowValuesWithAny.Reserve(RowValues.Num());
		for(bool Value : RowValues)
		{
			Column.RowValuesWithAny.Add(Value ? EBoolColumnCellValue::MatchTrue : EBoolColumnCellValue::MatchFalse);
		}
	}
};