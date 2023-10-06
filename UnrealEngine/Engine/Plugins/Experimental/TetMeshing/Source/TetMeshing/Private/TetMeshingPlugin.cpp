// Copyright Epic Games, Inc. All Rights Reserved.

#include "TetMeshingPlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(LogTetMeshing);


class FTetMeshingPlugin : public ITetMeshingPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FTetMeshingPlugin, TetMeshing )



void FTetMeshingPlugin::StartupModule()
{
	
}


void FTetMeshingPlugin::ShutdownModule()
{
	
}



