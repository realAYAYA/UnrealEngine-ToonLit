// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FCADLibraryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// #ueent_todo: startup external module (Coretech)
		// (currently in CADTranslator)
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FCADLibraryModule, CADLibrary);
