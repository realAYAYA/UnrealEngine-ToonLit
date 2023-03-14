// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class CHAOSCORE_API FChaosCoreEngineModule : public IModuleInterface
{
public:

	virtual void StartupModule() override {};
	virtual void ShutdownModule() override {};
};
