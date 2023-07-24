// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementRegistry.h"
#include "Modules/ModuleManager.h"

class FTypedElementFrameworkModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UTypedElementRegistry::Private_InitializeInstance();
	}

	virtual void ShutdownModule() override
	{
		UTypedElementRegistry::Private_ShutdownInstance();
	}
};

IMPLEMENT_MODULE(FTypedElementFrameworkModule, TypedElementFramework)
