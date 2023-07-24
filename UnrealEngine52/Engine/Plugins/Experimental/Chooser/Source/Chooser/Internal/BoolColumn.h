// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterBool.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "BoolColumn.generated.h"

USTRUCT(DisplayName = "Bool Property Binding")
struct CHOOSER_API FBoolContextProperty :  public FChooserParameterBoolBase
{
	GENERATED_BODY()
public:

	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	virtual bool GetValue(const UObject* ContextObject, bool& OutResult) const override;

#if WITH_EDITOR
	static bool CanBind(const FProperty& Property)
	{
		static FString BoolTypeName = "bool";
		return Property.GetCPPType() == BoolTypeName;
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
struct CHOOSER_API FBoolColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FBoolColumn();
	
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterBoolBase"), Category = "Hidden")
	FInstancedStruct InputValue;
	
	UPROPERTY(EditAnywhere, Category=Runtime);
	TArray<bool> RowValues; 
	
	virtual void Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const override;

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterBoolBase);
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
		Property.PropertyBindingChain = PropertyBindingChain;
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
		Column.RowValues = RowValues;
	}
};