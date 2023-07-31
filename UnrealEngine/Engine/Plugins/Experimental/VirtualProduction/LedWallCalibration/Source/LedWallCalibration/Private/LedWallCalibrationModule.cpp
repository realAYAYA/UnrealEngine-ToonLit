// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LedWallCalibrationLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "LedWallCalibration"

DEFINE_LOG_CATEGORY(LogLedWallCalibration);

class FLedWallCalibrationModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLedWallCalibrationModule, LedWallCalibration)
