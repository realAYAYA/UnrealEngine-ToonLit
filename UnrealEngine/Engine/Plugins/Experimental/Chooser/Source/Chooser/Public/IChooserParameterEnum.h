// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InstancedStruct.h"
#include "IChooserParameterBase.h"
#include "IChooserParameterEnum.generated.h"

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UChooserParameterEnum : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IChooserParameterEnum
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const {}
};

USTRUCT()
struct FChooserParameterEnumBase : public FChooserParameterBase
{
	GENERATED_BODY()
	
		virtual bool GetValue(FChooserEvaluationContext& Context, uint8& OutResult) const { return false; }
    	virtual bool SetValue(FChooserEvaluationContext& Context, uint8 InValue) const { return false; }

	#if WITH_EDITOR
    	virtual const UEnum* GetEnum() const { return nullptr; }
    #endif
};