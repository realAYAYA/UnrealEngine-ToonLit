// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "StateTreeToolMenuContext.generated.h"

class FStateTreeEditor;

UCLASS()
class STATETREEEDITORMODULE_API UStateTreeToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<FStateTreeEditor> StateTreeEditor;
};
