// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileAssetFactory.h"
#include "PhysicsControlProfileAsset.h"

//======================================================================================================================
UPhysicsControlProfileAssetFactory::UPhysicsControlProfileAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhysicsControlProfileAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = false;
}

//======================================================================================================================
UObject* UPhysicsControlProfileAssetFactory::FactoryCreateNew(
	UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPhysicsControlProfileAsset>(InParent, Class, Name, Flags, Context);
}
