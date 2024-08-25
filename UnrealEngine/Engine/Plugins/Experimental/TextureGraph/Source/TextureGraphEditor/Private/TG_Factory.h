// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "TG_Factory.generated.h"

UCLASS()
class UTG_Factory : public UFactory
{
	GENERATED_BODY()
public:
	UTG_Factory();
	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};
