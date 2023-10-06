// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "IChooserParameterBase.h"
#include "InstancedStruct.h"
#include "IChooserParameterBool.generated.h"

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UChooserParameterBool : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IChooserParameterBool
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const {}
};

USTRUCT()
struct FChooserParameterBoolBase : public FChooserParameterBase
{
	GENERATED_BODY()
    
    public:
    	virtual bool GetValue(FChooserEvaluationContext& Context, bool& OutResult) const { return false; }
    	virtual bool SetValue(FChooserEvaluationContext& Context, bool InValue) const { return false; }
};