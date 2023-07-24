// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeModule.h"

#define LOCTEXT_NAMESPACE "StateTree"

class FStateTreeModule : public IStateTreeModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FStateTreeModule, StateTreeModule)

void FStateTreeModule::StartupModule()
{
}

void FStateTreeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
