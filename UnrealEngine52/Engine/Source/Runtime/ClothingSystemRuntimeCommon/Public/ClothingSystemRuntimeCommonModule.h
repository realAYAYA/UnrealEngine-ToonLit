// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FClothingSystemRuntimeCommonModule : public IModuleInterface
{

public:

	FClothingSystemRuntimeCommonModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
};
