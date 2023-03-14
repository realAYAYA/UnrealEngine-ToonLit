// Copyright Epic Games, Inc. All Rights Reserved.
#include "AndroidDeviceProfileCommandlets.h"
#include "Modules/ModuleInterface.h"


class FAndroidDeviceProfileCommandlets : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE( FAndroidDeviceProfileCommandlets, AndroidDeviceProfileCommandlets )
