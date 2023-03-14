// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionsModule.h"
#include "Modules/ModuleManager.h"


//----------------------------------------------------------------------//
// IGameplayInteractionsModule
//----------------------------------------------------------------------//
IGameplayInteractionsModule& IGameplayInteractionsModule::Get()
{
	return FModuleManager::LoadModuleChecked<IGameplayInteractionsModule>("GameplayInteractionsModule");
}

bool IGameplayInteractionsModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("GameplayInteractionsModule");
}


//----------------------------------------------------------------------//
// FGameplayInteractionsModule
//----------------------------------------------------------------------//
class FGameplayInteractionsModule : public IGameplayInteractionsModule
{
};

IMPLEMENT_MODULE(FGameplayInteractionsModule, GameplayInteractionsModule)
