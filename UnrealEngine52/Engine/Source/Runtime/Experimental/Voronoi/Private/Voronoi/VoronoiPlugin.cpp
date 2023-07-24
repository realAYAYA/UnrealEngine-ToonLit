// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voronoi/VoronoiPlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FVoronoiPlugin : public IVoronoiPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FVoronoiPlugin, Voronoi )

void FVoronoiPlugin::StartupModule()
{
	
}


void FVoronoiPlugin::ShutdownModule()
{
	
}



