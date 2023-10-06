// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FTypedElementRuntimeModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked("TypedElementFramework");
	}
};

IMPLEMENT_MODULE(FTypedElementRuntimeModule, TypedElementRuntime)
