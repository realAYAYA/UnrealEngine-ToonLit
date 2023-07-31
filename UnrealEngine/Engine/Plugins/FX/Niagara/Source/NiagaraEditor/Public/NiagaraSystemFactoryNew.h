// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraTypes.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "NiagaraSystemFactoryNew.generated.h"

class UNiagaraSystem;
class UNiagaraEmitter;

UCLASS(hidecategories = Object)
class UNiagaraSystemFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	UNiagaraSystem* SystemToCopy = nullptr;
	TArray<FVersionedNiagaraEmitter> EmittersToAddToNewSystem;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

public:
	NIAGARAEDITOR_API static void InitializeSystem(UNiagaraSystem* System, bool bCreateDefaultNodes);
};



