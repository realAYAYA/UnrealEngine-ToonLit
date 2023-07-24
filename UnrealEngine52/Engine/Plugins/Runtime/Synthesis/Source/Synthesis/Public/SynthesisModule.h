// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSynthesis, Log, All);

class FSynthesisModule : public IModuleInterface
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};




#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Modules/ModuleManager.h"
#endif
