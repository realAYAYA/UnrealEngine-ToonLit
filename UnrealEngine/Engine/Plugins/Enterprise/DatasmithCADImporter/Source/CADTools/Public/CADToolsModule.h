// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"

#define CADTOOLS_MODULE_NAME TEXT("CADTools")

class CADTOOLS_API FCADToolsModule : public IModuleInterface
{
public:
	static FCADToolsModule& Get();
	static bool IsAvailable();

	static uint32 GetCacheVersion();
};
