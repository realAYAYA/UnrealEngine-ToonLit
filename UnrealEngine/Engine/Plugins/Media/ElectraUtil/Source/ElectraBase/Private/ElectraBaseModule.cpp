// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraBaseModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IElectraBaseModule.h"

#define LOCTEXT_NAMESPACE "ElectraBaseModule"

DEFINE_LOG_CATEGORY(LogElectraBase);

// -----------------------------------------------------------------------------------------------------------------------------------

class FElectraBaseModule: public IElectraBaseModule
{
public:
public:
	// IModuleInterface interface

	void StartupModule() override
	{
	}

	void ShutdownModule() override
	{
	}

private:
};

IMPLEMENT_MODULE(FElectraBaseModule, ElectraBase);

#undef LOCTEXT_NAMESPACE


