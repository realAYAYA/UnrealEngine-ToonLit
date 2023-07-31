// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraSettings.h"
#include "Factories/Factory.h"

#include "NiagaraEmitterFactoryNew.generated.h"


/** A factory for niagara emitter assets. */
UCLASS(hidecategories = Object, MinimalAPI)
class UNiagaraEmitterFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	UNiagaraEmitter* EmitterToCopy;
	bool bUseInheritance;
	bool bAddDefaultModulesAndRenderersToEmptyEmitter;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

public:
	static void InitializeEmitter(UNiagaraEmitter* NewEmitter, bool bAddDefaultModulesAndRenderers);
};



