// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FChaosEngineModule : public IModuleInterface
{
public:

	CHAOS_API virtual void StartupModule() override;
	CHAOS_API virtual void ShutdownModule() override;
};
