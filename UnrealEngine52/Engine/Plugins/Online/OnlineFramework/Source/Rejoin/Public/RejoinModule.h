// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/**
 * Module for Rejoin base implementation
 */
class FRejoinModule : 
	public IModuleInterface
{
private:
// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Modules/ModuleManager.h"
#endif
