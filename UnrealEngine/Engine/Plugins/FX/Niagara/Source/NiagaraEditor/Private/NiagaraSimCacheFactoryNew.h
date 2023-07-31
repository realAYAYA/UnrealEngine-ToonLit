// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "NiagaraSimCacheFactoryNew.generated.h"

class UNiagaraSimCache;

UCLASS(hidecategories=Object, MinimalAPI)
class UNiagaraSimCacheFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
