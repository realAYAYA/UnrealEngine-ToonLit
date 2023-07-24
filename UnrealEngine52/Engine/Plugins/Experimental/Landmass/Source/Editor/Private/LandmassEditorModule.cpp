// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandmassEditorModule.h"
#include "Modules/ModuleManager.h"


class FLandmassEditorModule : public ILandmassEditorModuleInterface
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

IMPLEMENT_MODULE(FLandmassEditorModule, LandmassEditor);

