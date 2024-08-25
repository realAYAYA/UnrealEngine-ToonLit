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

	CHOOSER_PARAMETER_BOILERPLATE();

#if WITH_EDITOR
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

	virtual void AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
	virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
#endif
	
	// FallbackValue will be used as the output value if the all rows in the chooser fail, and the FallbackResult from the chooser is used.
	UPROPERTY(EditAnywhere, Meta = (StructTypeConst), Category=Data);
   	FInstancedStruct FallbackValue;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Meta = (StructTypeConst), Category=Data);
	FInstancedStruct DefaultRowValue;
#endif
	
	UPROPERTY(EditAnywhere, Meta = (StructTypeConst), Category=Data);
	TArray<FInstancedStruct> RowValues; 

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterStructBase);
};