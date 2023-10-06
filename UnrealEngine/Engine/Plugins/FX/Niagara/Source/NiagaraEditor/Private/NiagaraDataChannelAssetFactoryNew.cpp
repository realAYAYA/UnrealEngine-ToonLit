// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelAssetFactoryNew.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "NiagaraSettings.h"
#include "NiagaraDataChannel.h"

UNiagaraDataChannelAssetFactoryNew::UNiagaraDataChannelAssetFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraDataChannelAsset::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraDataChannelAssetFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraDataChannelAsset::StaticClass()));

	UNiagaraDataChannelAsset* NewChannel = NewObject<UNiagaraDataChannelAsset>(InParent, Class, Name, Flags | RF_Transactional);

	return NewChannel;
}

bool UNiagaraDataChannelAssetFactoryNew::CanCreateNew()const
{
	return INiagaraModule::DataChannelsEnabled();
}
