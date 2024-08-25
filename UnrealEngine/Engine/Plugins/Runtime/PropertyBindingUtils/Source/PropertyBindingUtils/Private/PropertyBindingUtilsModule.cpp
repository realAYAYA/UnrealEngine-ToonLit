// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBindingUtilsModule.h"

#define LOCTEXT_NAMESPACE "PropertyBindingUtils"

class FPropertyBindingUtilsModule : public IPropertyBindingUtilsModule
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FPropertyBindingUtilsModule, PropertyBindingUtils)

void FPropertyBindingUtilsModule::StartupModule()
{
}

void FPropertyBindingUtilsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
