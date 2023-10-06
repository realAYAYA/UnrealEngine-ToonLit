// Copyright Epic Games, Inc. All Rights Reserved.

#include "FabModule.h"

class FFabModule : public IFabModule
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FFabModule, Fab);
