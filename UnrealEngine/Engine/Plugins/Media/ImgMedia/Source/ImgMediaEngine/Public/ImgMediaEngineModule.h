// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FImgMediaEngineModule : public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

