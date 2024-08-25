// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FZenModule
	: public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("Sockets"));
	}
};

IMPLEMENT_MODULE(FDefaultModuleImpl, Zen)
