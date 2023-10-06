// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterStruct.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "OutputStructColumn.generated.h"

struct FBindingChainElement;

USTRUCT(DisplayName = "Struct Property Binding")
struct CHOOSER_API FStructContextProperty : public FChooserParameterStructBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Meta = (BindingType = "struct", BindingColor = "StructPinTypeColor"), Category = "Binding")
	FChooserStructPropertyBinding Binding;

	virtual bool SetValue(FChooserEvaluationContext& Context, const FInstancedStruct &Value) const override;

#if WITH_EDITOR
	void SetBinding(const UObject* OuterObject, const TArray<FBindingChainElement>& InBindingChain);

	virtual void GetDisplayName(FText& OutName) const override
	{
		if (!Binding.DisplayName.IsEmpty())
		{
			OutName = FText::FromString(Binding.DisplayName);
		} 
		else if (!Binding.PropertyBindingChain.IsEmpty())
		{
			OutName = FText::FromName(Binding.PropertyBindingChain.Last());
		}
	}

	virtual UScriptStruct* GetStructType() const override { return Binding.StructType; }
#endif
};

USTRUCT()
struct CHOOSER_API FOutputStructColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FOutputStructColumn();
	virtual bool HasFilters() const override { return false; }
	virtual bool HasOutputs() const override { return true; }
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterStructBase"), Category = "Hidden")
	FInstancedStruct InputValue;

#if WITH_EDITOR
	void StructTypeChanged();
	mutable FInstancedStruct TestValue;
#endif
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Meta = (StructTypeConst), Category=Runtime);
	FInstancedStruct DefaultRowValue;
#endif
	
	UPROPERTY(EditAnywhere, Meta = (StructTypeConst), Category=Runtime);
	TArray<FInstancedStruct> RowValues; 

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterStructBase);
};