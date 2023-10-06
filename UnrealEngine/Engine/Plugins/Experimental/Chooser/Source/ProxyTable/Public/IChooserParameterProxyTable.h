// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IChooserParameterBase.h"
#include "InstancedStruct.h"
#include "IObjectChooser.h"
#include "IChooserParameterProxyTable.generated.h"

class UProxyTable;

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class PROXYTABLE_API UChooserParameterProxyTable : public UInterface
{
	GENERATED_BODY()
};

class PROXYTABLE_API IChooserParameterProxyTable
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const {}
};


USTRUCT()
struct FChooserParameterProxyTableBase : public FChooserParameterBase
{
	GENERATED_BODY()
    
public:
	virtual bool GetValue(FChooserEvaluationContext& Context, const UProxyTable*& OutResult) const { return false; }
};