// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshPaintingToolset.h"
#include "Modules/ModuleManager.h"



#define LOCTEXT_NAMESPACE "MeshPaintMode"

class FMeshPaintingToolsetModule : public IModuleInterface
{
public:
	FMeshPaintingToolsetModule()
	{
	}

	// FModuleInterface overrides
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:


};


void FMeshPaintingToolsetModule::StartupModule()
{
}


void FMeshPaintingToolsetModule::ShutdownModule()
{
}




IMPLEMENT_MODULE(FMeshPaintingToolsetModule, MeshPaintingToolset)

#undef LOCTEXT_NAMESPACE
