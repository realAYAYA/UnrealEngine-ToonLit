// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"

class FAudioExtensionsModule final : public IModuleInterface
{
public:
	static FAudioExtensionsModule* Get(); 
	
	virtual void StartupModule() override;
};