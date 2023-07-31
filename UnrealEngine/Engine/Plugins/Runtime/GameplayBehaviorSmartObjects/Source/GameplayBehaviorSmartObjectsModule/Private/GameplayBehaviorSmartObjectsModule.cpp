// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayBehaviorSmartObjectsModule.h"
#include "Modules/ModuleManager.h"


//----------------------------------------------------------------------//
// IGameplayBehaviorSmartObjectsModule
//----------------------------------------------------------------------//
IGameplayBehaviorSmartObjectsModule& IGameplayBehaviorSmartObjectsModule::Get()
{
	return FModuleManager::LoadModuleChecked<IGameplayBehaviorSmartObjectsModule>("GameplayBehaviorSmartObjectsModule");
}

bool IGameplayBehaviorSmartObjectsModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("GameplayBehaviorSmartObjectsModule");
}


//----------------------------------------------------------------------//
// FGameplayBehaviorSmartObjectsModule
//----------------------------------------------------------------------//
class FGameplayBehaviorSmartObjectsModule : public IGameplayBehaviorSmartObjectsModule
{
};

IMPLEMENT_MODULE(FGameplayBehaviorSmartObjectsModule, GameplayBehaviorSmartObjectsModule)
