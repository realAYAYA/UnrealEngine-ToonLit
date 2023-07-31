// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

GOOGLEPAD_API DECLARE_LOG_CATEGORY_EXTERN(LogGooglePAD, Log, All);

class FGooglePADModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
