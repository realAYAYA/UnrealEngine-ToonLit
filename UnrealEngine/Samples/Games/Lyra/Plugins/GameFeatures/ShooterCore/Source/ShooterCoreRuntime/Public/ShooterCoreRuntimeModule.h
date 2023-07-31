// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class UShooterCoreRuntimeSettings;

class FShooterCoreRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
protected:

	UShooterCoreRuntimeSettings* ShooterCoreSettings = nullptr;
};
