// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterFloat.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
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

#if WITH_EDITOR
	static bool CanBind(const FProperty& Property)
	{
		static FString DoubleTypeName = "double";
		static FString FloatTypeName = "float";
		const FString& TypeName = Property.GetCPPType();
		return TypeName == FloatTypeName || TypeName == DoubleTypeName;
	}
	
	void SetBinding(const TArray<FBindingChainElement>& InBindingChain)
	{
		UE::Chooser::CopyPropertyChain(InBindingChain, Binding);
	}

	virtual void GetDisplayName(FText& OutName) const override
	{
		if (!Binding.PropertyBindingChain.IsEmpty())
		{
			OutName = FText::FromName(Binding.PropertyBindingChain.Last());
		}
	}
#endif
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
	
	virtual void Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const override;

#if WITH_EDITOR
	mutable float TestValue;
	virtual bool EditorTestFilter(int32 RowIndex) const override
	{
		return RowValues.IsValidIndex(RowIndex) && TestValue >= RowValues[RowIndex].Min && TestValue <= RowValues[RowIndex].Max;
	}
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