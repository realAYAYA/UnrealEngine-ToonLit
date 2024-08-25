// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FNiagaraBlueprintNodesModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{}

	virtual void ShutdownModule() override
	{}
};

IMPLEMENT_MODULE(FNiagaraBlueprintNodesModule, NiagaraBlueprintNodes);