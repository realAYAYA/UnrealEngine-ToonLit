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
	
	virtual bool GetValue(const UObject* ContextObject, float& OutResult) const override;

	UPROPERTY()
	TArray<FName> PropertyBindingChain;

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
		UE::Chooser::CopyPropertyChain(InBindingChain, PropertyBindingChain);
	}

	virtual void GetDisplayName(FText& OutName) const override
	{
		if (!PropertyBindingChain.IsEmpty())
		{
			OutName = FText::FromName(PropertyBindingChain.Last());
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
		
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterFloatBase"), Category = "Hidden")
	FInstancedStruct InputValue;
	
	UPROPERTY(EditAnywhere, Category=Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<FChooserFloatRangeRowData> RowValues;
	
	virtual void Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const override;
	
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
		Property.PropertyBindingChain = PropertyBindingChain;
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