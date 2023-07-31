// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusDeveloperModule.h"

class FOptimusDeveloperModule : public IOptimusDeveloperModule
{
public:
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogOptimusDeveloper, Log, All);
