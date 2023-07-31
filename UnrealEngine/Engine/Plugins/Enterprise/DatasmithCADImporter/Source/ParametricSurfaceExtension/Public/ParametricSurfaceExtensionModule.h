// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#define PARAMETRICSURFACEEXTENSION_MODULE_NAME TEXT("ParametricSurfaceExtension")

/**
 * This module exposes additional features for assets containing CoreTech data.
 */
class FParametricSurfaceExtensionModule : public IModuleInterface
{
public:
    static FParametricSurfaceExtensionModule& Get();
    static bool IsAvailable();

private:
	virtual void StartupModule() override;
};
