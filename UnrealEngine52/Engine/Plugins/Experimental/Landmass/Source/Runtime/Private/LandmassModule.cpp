// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandmassModule.h"
#include "Modules/ModuleManager.h"


class FLandmassModule : public ILandmassModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FLandmassModule, Landmass);

