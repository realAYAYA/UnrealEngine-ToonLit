// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "NNEQAParametricTest.h"

class FNNEQAModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		UE::NNEQA::Private::InitializeParametricTests();
	}

	virtual void ShutdownModule() override
	{
	}

};

IMPLEMENT_MODULE(FNNEQAModule, NNEQA);