// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMobileFSR, Verbose, All);

class FMobileFSRViewExtension;

class FMobileFSRModule final : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
	void OnPostEngineInit();

	TSharedPtr<FMobileFSRViewExtension, ESPMode::ThreadSafe> ViewExtension;
};