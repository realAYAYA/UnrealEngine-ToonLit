// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#define CADKERNELSURFACE_MODULE_NAME TEXT("CADKernelSurface")

/**
 * This module exposes additional features for assets containing CADKernel data.
 */
class FCADKernelSurfaceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	static FCADKernelSurfaceModule& Get();
	static bool IsAvailable();
};
