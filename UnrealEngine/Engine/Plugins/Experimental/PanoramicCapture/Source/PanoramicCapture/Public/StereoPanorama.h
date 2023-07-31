// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FStereoPanoramaManager;

/**
 * Implements the StereoPanorama module.
 */
class FStereoPanoramaModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	static TSharedPtr<FStereoPanoramaManager> Get();
};
