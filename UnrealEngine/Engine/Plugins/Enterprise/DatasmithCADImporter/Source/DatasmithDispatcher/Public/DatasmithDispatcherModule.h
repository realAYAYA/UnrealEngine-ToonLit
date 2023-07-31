// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"

#define DATASMITHDISPATCHER_MODULE_NAME TEXT("DatasmithDispatcher")

/**
 * This module exposes additional features for assets containing CoreTech data.
 */
class FDatasmithDispatcherModule : public IModuleInterface
{
public:
	static FDatasmithDispatcherModule& Get();
	static bool IsAvailable();

private:
	virtual void StartupModule() override;
};
