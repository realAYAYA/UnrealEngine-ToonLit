// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Framework/Animation/AnimatedAttributeManager.h"


/**
 * Implements the Slate module.
 */
class FSlateModule
	: public IModuleInterface
{
	virtual void StartupModule() override
	{
		FAnimatedAttributeManager::Get().SetupTick();
	}

	virtual void ShutdownModule() override
	{
		FAnimatedAttributeManager::Get().TeardownTick();
	}
};


IMPLEMENT_MODULE(FSlateModule, Slate);
