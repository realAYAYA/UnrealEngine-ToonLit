// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantManagerContentModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "VariantManagerContentModule"


class FVariantManagerContentModule : public IVariantManagerContentModule
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FVariantManagerContentModule, VariantManagerContent);

#undef LOCTEXT_NAMESPACE