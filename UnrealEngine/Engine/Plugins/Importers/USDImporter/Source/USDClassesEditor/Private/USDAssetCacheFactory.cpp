// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDAssetCacheFactory.h"

#include "USDAssetCache2.h"

UUsdAssetCacheFactory::UUsdAssetCacheFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UUsdAssetCache2::StaticClass();
}

UObject* UUsdAssetCacheFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
)
{
	return NewObject<UUsdAssetCache2>(InParent, Name, Flags | RF_Transactional | RF_Public | RF_Standalone);
}

bool UUsdAssetCacheFactory::ShouldShowInNewMenu() const
{
	return true;
}
