// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "NNEUtilitiesORTIncludeHelper.h"

class FNNEUtilitiesModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		Ort::InitApi();
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FNNEUtilitiesModule, NNEUtilities)