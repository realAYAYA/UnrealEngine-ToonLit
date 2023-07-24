// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeEditorModule.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(LogXRCreativeEditor);


class FXRCreativeEditorModule : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


IMPLEMENT_MODULE(FXRCreativeEditorModule, XRCreativeEditor);
