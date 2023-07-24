// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelDefinitionsFactoryNew.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "NiagaraSettings.h"
#include "NiagaraDataChannelDefinitions.h"

UNiagaraDataChannelDefinitionsFactoryNew::UNiagaraDataChannelDefinitionsFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraDataChannelDefinitions::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraDataChannelDefinitionsFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraDataChannelDefinitions::StaticClass()));

	UNiagaraDataChannelDefinitions* NewDefinitions = NewObject<UNiagaraDataChannelDefinitions>(InParent, Class, Name, Flags | RF_Transactional);

	return NewDefinitions;
}

bool UNiagaraDataChannelDefinitionsFactoryNew::CanCreateNew()const
{
	return INiagaraModule::DataChannelsEnabled();
}
