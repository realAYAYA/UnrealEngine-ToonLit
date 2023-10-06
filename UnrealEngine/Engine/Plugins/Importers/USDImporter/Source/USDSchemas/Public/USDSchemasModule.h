// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FUsdRenderContextRegistry;
class FUsdSchemaTranslatorRegistry;

class IUsdSchemasModule : public IModuleInterface
{
public:
	virtual FUsdSchemaTranslatorRegistry& GetTranslatorRegistry() = 0;
	virtual FUsdRenderContextRegistry& GetRenderContextRegistry() = 0;
};
