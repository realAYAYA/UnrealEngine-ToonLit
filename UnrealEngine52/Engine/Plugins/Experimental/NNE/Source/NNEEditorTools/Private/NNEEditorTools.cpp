// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FNNEEditorToolsModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

};

IMPLEMENT_MODULE(FNNEEditorToolsModule, NNEEditorTools)