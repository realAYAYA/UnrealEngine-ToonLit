// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheFactoryNew.h"
#include "NiagaraSimCache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSimCacheFactoryNew)

UNiagaraSimCacheFactoryNew::UNiagaraSimCacheFactoryNew(const FObjectInitializer& ObjectInitializer)
{
	SupportedClass = UNiagaraSimCache::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UNiagaraSimCacheFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}

UObject* UNiagaraSimCacheFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UNiagaraSimCache>(InParent, InClass, InName, Flags);
}

