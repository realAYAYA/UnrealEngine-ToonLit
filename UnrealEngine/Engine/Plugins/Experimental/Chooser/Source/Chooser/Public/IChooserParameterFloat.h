// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "InstancedStruct.h"
#include "IChooserParameterBase.h"
#include "IChooserParameterFloat.generated.h"

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UChooserParameterFloat : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IChooserParameterFloat
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const {}
};

USTRUCT()
struct FChooserParameterFloatBase : public FChooserParameterBase
{
	GENERATED_BODY()
    
public:
	virtual bool GetValue(FChooserEvaluationContext& Context, double& OutResult) const { return false; }
    virtual bool SetValue(FChooserEvaluationContext& Context, double InValue) const { return false; }
};