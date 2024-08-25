// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/CameraAssetFactory.h"

#include "Core/CameraAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraAssetFactory)

#define LOCTEXT_NAMESPACE "CameraAssetFactory"

UCameraAssetFactory::UCameraAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UCameraAsset::StaticClass();
}

UObject* UCameraAssetFactory::FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCameraAsset* NewCameraAsset = NewObject<UCameraAsset>(Parent, Class, Name, Flags | RF_Transactional);
	return NewCameraAsset;
}

bool UCameraAssetFactory::ConfigureProperties()
{
	return true;
}

bool UCameraAssetFactory::ShouldShowInNewMenu() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE


