// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
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


USTRUCT()
struct CHOOSER_API FChooserColumnBase
{
	GENERATED_BODY()

public:
	virtual ~FChooserColumnBase() {}
	virtual void PostLoad() {};
	virtual void Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const {}

#if WITH_EDITOR
	virtual void SetNumRows(uint32 NumRows) {}
	virtual void DeleteRows(const TArray<uint32> & RowIndices) {}
	
	virtual UScriptStruct* GetInputBaseType() const { return nullptr; };
	virtual const UScriptStruct* GetInputType() const { return nullptr; };
	virtual void SetInputType(const UScriptStruct* Type) { };
	virtual FChooserParameterBase* GetInputValue() { return nullptr; };
#endif
};

#if WITH_EDITOR
#define CHOOSER_COLUMN_BOILERPLATE_NoSetInputType(ParameterType) \
	virtual void SetNumRows(uint32 NumRows) override { RowValues.SetNum(NumRows); }\
	virtual void DeleteRows(const TArray<uint32> & RowIndices )\
	{\
		for(uint32 Index : RowIndices)\
		{\
			RowValues.RemoveAt(Index);\
		}\
	}\
	virtual UScriptStruct* GetInputBaseType() const override { return ParameterType::StaticStruct(); };\
	virtual const UScriptStruct* GetInputType() const override { return InputValue.IsValid() ? InputValue.GetScriptStruct() : nullptr; };\
	virtual FChooserParameterBase* GetInputValue() override { return InputValue.IsValid() ? &InputValue.GetMutable<FChooserParameterBase>() : nullptr; };
	
#define CHOOSER_COLUMN_BOILERPLATE(ParameterType) \
	CHOOSER_COLUMN_BOILERPLATE_NoSetInputType(ParameterType); \
	virtual void SetInputType(const UScriptStruct* Type) override { InputValue.InitializeAs(Type); };

#else

#define CHOOSER_COLUMN_BOILERPLATE(ParameterType)
#define CHOOSER_COLUMN_BOILERPLATE_NoSetInputType(ParameterType)

#endif
