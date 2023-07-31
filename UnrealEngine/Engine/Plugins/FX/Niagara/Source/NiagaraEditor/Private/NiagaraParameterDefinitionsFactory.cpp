// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterDefinitionsFactory.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "NiagaraSettings.h"
#include "NiagaraParameterDefinitions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraParameterDefinitionsFactory)

UNiagaraParameterDefinitionsFactory::UNiagaraParameterDefinitionsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraParameterDefinitions::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraParameterDefinitionsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraParameterDefinitions::StaticClass()));

	UNiagaraParameterDefinitions* NewLibrary = NewObject<UNiagaraParameterDefinitions>(InParent, Class, Name, Flags | RF_Transactional);

	return NewLibrary;
}

