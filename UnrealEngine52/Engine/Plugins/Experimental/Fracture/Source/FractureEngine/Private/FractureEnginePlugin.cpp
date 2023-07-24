// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureEnginePlugin.h"




class FFractureEnginePlugin : public IFractureEnginePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FFractureEnginePlugin, FractureEngine )



void FFractureEnginePlugin::StartupModule()
{

}


void FFractureEnginePlugin::ShutdownModule()
{

}



