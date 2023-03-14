// Copyright Epic Games, Inc. All Rights Reserved.

#include "AlembicImporterModule.h"
#include "AlembicLibraryModule.h"

class FAlembicImporterModule : public IAlembicImporterModuleInterface
{
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked< IAlembicLibraryModule >("AlembicLibrary");
	}
};

IMPLEMENT_MODULE(FAlembicImporterModule, AlembicImporter);