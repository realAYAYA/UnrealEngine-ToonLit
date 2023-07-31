// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class ILowLevelTestsModule : public IModuleInterface
{
public:
	virtual void GlobalSetup() = 0;
	virtual void GlobalTeardown() = 0;
};
