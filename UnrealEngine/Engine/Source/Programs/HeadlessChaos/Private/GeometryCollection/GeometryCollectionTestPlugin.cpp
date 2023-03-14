// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionTestPlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FGeometryCollectionTestPlugin : public IGeometryCollectionTestPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FGeometryCollectionTestPlugin, GeometryCollectionTestCore)



void FGeometryCollectionTestPlugin::StartupModule()
{

}


void FGeometryCollectionTestPlugin::ShutdownModule()
{

}
