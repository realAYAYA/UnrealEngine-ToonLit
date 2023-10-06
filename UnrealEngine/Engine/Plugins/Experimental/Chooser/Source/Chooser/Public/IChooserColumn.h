// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "IObjectChooser.h"
#include "IChooserParameterBase.h"
#include "InstancedStruct.h"
#include "IChooserColumn.generated.h"

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UChooserColumn : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IChooserColumn 
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const {}
};

class UChooserTable;

USTRUCT()
struct CHOOSER_API FChooserColumnBase
{
	GENERATED_BODY()

public:
	virtual ~FChooserColumnBase() {}
	virtual void PostLoad() {};
	virtual void Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const {}

	virtual bool HasFilters() const { return true; }
	virtual bool HasOutputs() const { return false; }
	
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const { }

#if WITH_EDITOR
	virtual FName RowValuesPropertyName() { return FName(); }
	virtual void SetNumRows(int32 NumRows) {}
	virtual void DeleteRows(const TArray<uint32> & RowIndices) {}
	virtual void MoveRow(int SourceIndex, int TargetIndex) {}
	
	virtual UScriptStruct* GetInputBaseType() const { return nullptr; };
	virtual const UScriptStruct* GetInputType() const { return nullptr; };
	virtual void SetInputType(const UScriptStruct* Type) { };
	virtual FChooserParameterBase* GetInputValue() { return nullptr; };

	// random columns must go last, and get a special icon
	// using a virtual fucntion to identify them (rather than hard coding a specific type) to potentially support multiple varieties of randomization column.
	virtual bool IsRandomizeColumn() const { return false; }

	virtual bool EditorTestFilter(int32 RowIndex) const { return false; }
#endif
};

#if WITH_EDITOR
#define CHOOSER_COLUMN_BOILERPLATE2(ParameterType, RowValuesProperty) \
	virtual FName RowValuesPropertyName() override { return #RowValuesProperty; }\
	virtual void SetNumRows(int32 NumRows) override\
	{\
		if (NumRows <= RowValuesProperty.Num())\
		{\
			RowValuesProperty.SetNum(NumRows);\
		}\
		else\
		{\
			while(RowValuesProperty.Num() < NumRows)\
			RowValuesProperty.Add(DefaultRowValue);\
		}\
	}\
	virtual void DeleteRows(const TArray<uint32> & RowIndices )\
	{\
		for(uint32 Index : RowIndices)\
		{\
			RowValuesProperty.RemoveAt(Index);\
		}\
	}\
	virtual void MoveRow(int SourceRowIndex, int TargetRowIndex)\
	{\
		auto RowData = RowValuesProperty[SourceRowIndex];\
    	RowValuesProperty.RemoveAt(SourceRowIndex);\
    	if (SourceRowIndex < TargetRowIndex) { TargetRowIndex--; }\
    	RowValuesProperty.Insert(RowData, TargetRowIndex);\
	}\
	virtual UScriptStruct* GetInputBaseType() const override { return ParameterType::StaticStruct(); };\
	virtual const UScriptStruct* GetInputType() const override { return InputValue.IsValid() ? InputValue.GetScriptStruct() : nullptr; };\
	virtual FChooserParameterBase* GetInputValue() override { return InputValue.IsValid() ? &InputValue.GetMutable<FChooserParameterBase>() : nullptr; };\
	virtual void SetInputType(const UScriptStruct* Type) override { InputValue.InitializeAs(Type); };

#define CHOOSER_COLUMN_BOILERPLATE(ParameterType) CHOOSER_COLUMN_BOILERPLATE2(ParameterType, RowValues)

#else

#define CHOOSER_COLUMN_BOILERPLATE2(ParameterType, RowValuesProperty)
#define CHOOSER_COLUMN_BOILERPLATE(ParameterType)

#endif
