// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraAssetTagDefinitionsFactory.h"

#include "NiagaraAssetTagDefinitions.h"

UNiagaraAssetTagDefinitionsFactory::UNiagaraAssetTagDefinitionsFactory()
{
	SupportedClass = UNiagaraAssetTagDefinitions::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraAssetTagDefinitionsFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UNiagaraAssetTagDefinitions* NewAssetTagDefinitions = NewObject<UNiagaraAssetTagDefinitions>(InParent, InClass, InName, Flags | RF_Transactional);
	return NewAssetTagDefinitions;
}
